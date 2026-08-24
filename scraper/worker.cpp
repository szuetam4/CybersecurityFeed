#include <iostream>
#include <string>
#include <sqlite3.h>
#include <curl/curl.h>
#include <vector>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

class Database {
private:
    sqlite3* db;
public:
    Database(const std::string& dbPath) {
        if(sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK){
            std::cerr << "[ERROR][DB] Błąd bazy: " << sqlite3_errmsg(db) << std::endl;
            db = nullptr;
        } else {
            std::cout << "[SUCCESS][DB] Połączono z SQLite." << std::endl;
        }
    }
    ~Database(){
        if(db) sqlite3_close(db);
    }

    sqlite3* get() const {return db; }
    bool isConnected() const {return db != nullptr;}
};

struct Article {
    std::string img;
    std::string link;
    std::string title;
    std::string tag;
};

struct Source {
  std::string name;
  std::string url;
};

void loadSourcesToDB(sqlite3* db, const std::vector<Source>& initialSources){
  if(!db) return;

  const char* sql = "INSERT OR IGNORE INTO sources (name, url) VALUES (?, ?);";
  sqlite3_stmt* stmt;

  if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
    std::cerr << "[ERROR] Błąd przygotowania zapytania startowego: " << sqlite3_errmsg(db) << std::endl;
    return;
  }

  sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
  int addedCount = 0;

  for(const auto& src : initialSources) {
    sqlite3_bind_text(stmt, 1, src.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, src.url.c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) == SQLITE_DONE){
      if(sqlite3_changes(db) > 0){
        addedCount++;
      }
      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);
    }
  }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_finalize(stmt);

    std::cout << "[INFO][DB] Inicjalizacja źródeł. Dodano nowych: " << addedCount << std::endl;
}

std::vector<Source> loadSourcesFromDB(sqlite3* db) {
  std::vector<Source> sources;
  if(!db) return sources;

  const char* sql = "SELECT id, name, url FROM sources;";
  sqlite3_stmt* stmt;

  if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
    std::cerr << "[ERROR] Bład wczytywania źródeł: " << sqlite3_errmsg(db) << std::endl;
    return sources;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW){
    Source src;
    src.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    src.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

    sources.push_back(src);
  }

  sqlite3_finalize(stmt);
  std::cout<< "[INFO][DB] Pobrano " << sources.size() << " żródeł do scrapowania." << std::endl;

  return sources;
}

std::vector<Source> loadSourcesFromJson(const std::string& filepath){
  std::vector<Source> sources;
  std::ifstream file(filepath);

  if(!file.is_open()){
    std::cerr << "[ERROR][FILE] Nie można otworzyć: " << filepath << std::endl;
    return sources;
  }
  try {
    json json;
    file >> json;
    for(const auto& item : json["websites"]){
      sources.push_back({
          item["name"].get<std::string>(),
          item["url"].get<std::string>()
      });
    }
  } catch (const json::exception& e){
    std::cerr << "[ERROR][JSON] Bład parsowania źródeł: " << e.what() << std::endl;
  }
  return sources;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp){
    size_t realSize = size * nmemb;
    std::string* mem = static_cast<std::string*>(userp);
    mem->append(static_cast<char*>(contents), realSize);
    return realSize;
}

std::string fetchHTML(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "[ERROR][CURL] curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

std::string linkExtraction(const std::string& extractedContent, const std::string& patternStart, const std::string& patternEnd){
    size_t startPos = extractedContent.find(patternStart, 0);
    startPos += patternStart.length();
    size_t endPos = extractedContent.find(patternEnd, startPos);
     
    size_t contentLength = endPos - startPos;
    std::string extractedLink = extractedContent.substr(startPos, contentLength);
    return extractedLink;
}

std::string getSuffix(const std::string& filepath, const std::string& sourceName, const std::string& suffix){
  std::ifstream file(filepath);

  if (!file.is_open()){
    std::cerr << "[ERROR][FILE] Nie można otworzyć pliku." << std::endl;
    return "";
  }
  
  try {
    json config;
    file >> config;

    if(config.contains("sources") &&
       config["sources"].contains(sourceName) &&
       config["sources"][sourceName].contains(suffix)) {

      return config["sources"][sourceName][suffix].get<std::string>();
    }
  } catch (const json::exception& e) {
    std::cerr << "[ERROR][JSON] Bład parsowania: " << e.what() << std::endl;
  }
  return "";
}

std::string fetchMoreHTML(const std::string& apiUrl, int pageNumber, const std::string& origin, const std::string& referer, const std::string& suffixSource, const std::string& suffixType) {
  CURL* curl;
  CURLcode res;
  std::string readBuffer;

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    std::string payloadPrefix = "action=csco_ajax_load_more&page=";
    std::string payloadSuffix = getSuffix("../scraper/resources/request_data.json", suffixSource, suffixType); 
    
    std::string postData = payloadPrefix + std::to_string(pageNumber) + payloadSuffix;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, ("Origin: " + origin).c_str());
    headers = curl_slist_append(headers, ("Referer: " + referer).c_str());
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    res = curl_easy_perform(curl);

    if(res != CURLE_OK){
      std::cerr << "[ERROR][CURL] curl fetch failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }

  return readBuffer;
}

std::string getCategoryTag(const std::string& content, const std::string& patternStart, const std::string& patternEnd){
  std::string parsedTag = linkExtraction(content, patternStart, patternEnd);
  if(parsedTag.empty() || parsedTag.length() > 20){
    std::cerr << "[DEBUG] Wyciągnięty tag wygląda błędnie: " << parsedTag << std::endl;
    return "uncategorized";
  } else {
    std::transform(parsedTag.begin(), parsedTag.end(), parsedTag.begin(),[](unsigned char c){return std::tolower(c);});
    return parsedTag;
  }
}
std::vector<Article> parseNetGuardia(const std::string& htmlContent, bool isFirstFetch) {
    std::vector<Article> articles;

    ////////////////////////////////////////////////////////////
    ////////PATTERN////SECTION////START/////////////////////////
    ////////////////////////////////////////////////////////////

    std::string startTag = "<article class=";
    std::string endTag = "</article>";
  
    std::string imgPatternStart = "data-src=\"";
    std::string imgPatternEnd = ".webp\"";

    std::string imgSecondPatternStart = "src=\"";
    std::string imgSecondPatternEnd = ".webp\"";
   
    std::string linkPatternStart = "href=\"";
    std::string linkPatternEnd = "/\"";
    
    std::string titlePatternStart = "title=\"";
    std::string titlePatternEnd = "\">";

    std::string tagPatternStart = "<title>";
    std::string tagPatternEnd = " | netguardia.com</title>";

    ////////////////////////////////////////////////////////////
    ////////PATTERN////SECTION////END///////////////////////////
    ////////////////////////////////////////////////////////////

    size_t searchPosition = 0;
    std::string tag = getCategoryTag(htmlContent, tagPatternStart, tagPatternEnd);

    while(true){
      size_t startPos = htmlContent.find(startTag, searchPosition);
  	  if(startPos == std::string::npos){break;}

	    startPos += startTag.length();

	    size_t endPos = htmlContent.find(endTag, startPos);
	    if (endPos == std::string::npos){break;}

      size_t contentLength = endPos - startPos;
	    std::string extractedContent = htmlContent.substr(startPos, contentLength);
      std::string imgLink = "";

      if(isFirstFetch == true){
        imgLink = linkExtraction(extractedContent, imgPatternStart, imgPatternEnd);
        imgLink += ".webp";
      } else {
        imgLink = linkExtraction(extractedContent, imgSecondPatternStart, imgSecondPatternEnd);
        imgLink += ".webp";
      }

      std::string link = linkExtraction(extractedContent, linkPatternStart, linkPatternEnd);
      std::string title = linkExtraction(extractedContent, titlePatternStart, titlePatternEnd);

	    Article article;
	    article.img = imgLink;
	    article.link = link;
      article.title = title;
      article.tag = tag;

	    articles.push_back(article);

	    searchPosition = endPos + endTag.length();
    }
    return articles;
}

void saveArticlesToDatabse(sqlite3* db, const std::vector<Article>& articles, int sourceId){
  if(!db){
    std::cerr << "[ERROR][DB] Brak połączenia z bazą podczas zapisu!" << std::endl;
    return;
  }

  sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

  const char* sqlArticle = "INSERT OR IGNORE INTO articles (title, link, img_url, published_at, source_id) VALUES (?, ?, ?, datetime('now', 'localtime'), ?);";
  const char* sqlTagInsert = "INSERT OR IGNORE INTO categories (name) VALUES (?);";
  const char* sqlTagSelect = "SELECT id FROM categories WHERE name = ?;";
  const char* sqlBridge = "INSERT INTO article_category (article_id, category_id) VALUES (?, ?);";
  
  sqlite3_stmt* stmtArticle = nullptr;
  sqlite3_stmt* stmtTagInsert = nullptr;
  sqlite3_stmt* stmtTagSelect = nullptr;
  sqlite3_stmt* stmtBridge = nullptr;

  if(sqlite3_prepare_v2(db, sqlArticle, -1, &stmtArticle, nullptr) != SQLITE_OK ||
     sqlite3_prepare_v2(db, sqlTagInsert, -1, &stmtTagInsert, nullptr) != SQLITE_OK ||
     sqlite3_prepare_v2(db, sqlTagSelect, -1, &stmtTagSelect, nullptr) != SQLITE_OK ||
     sqlite3_prepare_v2(db, sqlBridge, -1, &stmtBridge, nullptr) != SQLITE_OK){
    
    std::cerr << "[ERROR][SQL] Błąd przygotowania SQL: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmtArticle);
    sqlite3_finalize(stmtTagInsert);
    sqlite3_finalize(stmtTagSelect);
    sqlite3_finalize(stmtBridge);
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return;
  }

  int insertedCount = 0;

  for(const auto& article : articles){
    std::string currentTag = article.tag.empty() ? "Uncategorized" : article.tag;

    sqlite3_bind_text(stmtTagInsert, 1, currentTag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmtTagInsert);
    sqlite3_reset(stmtTagInsert);
    sqlite3_clear_bindings(stmtTagInsert);

    int tagId = 0;
    sqlite3_bind_text(stmtTagSelect, 1, currentTag.c_str(), -1, SQLITE_TRANSIENT);
    if(sqlite3_step(stmtTagSelect) == SQLITE_ROW){
      tagId = sqlite3_column_int(stmtTagSelect, 0);
    }
    sqlite3_reset(stmtTagSelect);
    sqlite3_clear_bindings(stmtTagSelect);
    
    if(tagId == 0){
      std::cerr << "[WARNING] Nie udało się ustalić ID dla kategorii: " << currentTag << std::endl;
      continue;
    }

    sqlite3_bind_text(stmtArticle, 1, article.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtArticle, 2, article.link.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtArticle, 3, article.img.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmtArticle, 4, sourceId);

    if(sqlite3_step(stmtArticle) == SQLITE_DONE) {
      if(sqlite3_changes(db) > 0){
        insertedCount++;

        int articleId = sqlite3_last_insert_rowid(db);

        sqlite3_bind_int(stmtBridge, 1, articleId);
        sqlite3_bind_int(stmtBridge, 2, tagId);

        if(sqlite3_step(stmtBridge) != SQLITE_DONE){
          std::cerr << "[WARNING] Błąd łaczenia taga z artykułem (" << article.title << "): " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_reset(stmtBridge);
        sqlite3_clear_bindings(stmtBridge);
      }
    } else {
      std::cerr << "[WARNING] Problem z artykułem: " << article.title
        << "\n[CAUSE]: " << sqlite3_errmsg(db) << "\n" << std::endl;
    }

    sqlite3_reset(stmtArticle);
    sqlite3_clear_bindings(stmtArticle);
  }

  sqlite3_finalize(stmtTagInsert);
  sqlite3_finalize(stmtTagSelect);
  sqlite3_finalize(stmtArticle);
  sqlite3_finalize(stmtBridge);

  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

  std::cout << "\n========================================" << std::endl;
  std::cout << "[DB] Baza zaktualizowana pomyślnie!" << std::endl;
  std::cout << "[DB] Dodano NOWYCH artykułów: " << insertedCount << std::endl;
  std::cout << "[DB] Zignorowano starych: " << (articles.size() - insertedCount) << std::endl;
  std::cout << "========================================\n" << std::endl;
}

std::vector<Article> getNetguardiaArticles(std::vector<Source>& netguardiaSources){
  std::vector<Article> allArticles;
  
  for(const auto& sourceLink : netguardiaSources){
    std::cout << "\n[INFO] [I]: Pobieranie kodu HTML z " << sourceLink.url << "..." << std::endl;

    std::string mainHtml = fetchHTML(sourceLink.url);
    std::cout << "[SUCCESS][CURL] Pobrano HTML'a o długości: " << mainHtml.length() << " znaków." << std::endl;

    std::vector<Article> mainArticles = parseNetGuardia(mainHtml, true);
    std::cout << "[SUCCESS] Znaleziono " << mainArticles.size() << " artykułów na netguardii" << std::endl;

    allArticles.insert(allArticles.end(), mainArticles.begin(), mainArticles.end());

    std::string ajaxUrl = "https://netguardia.com/wp-json/csco/v1/more-posts";
    int currentPage = 2;
    bool hasMorePosts = true;
    ////////////////CAUTION!!!//////////////////////
    int maxPagesToFetch = 5; //HARDLIMITER
    /*If set to 0, program will fetch all posible content*/
    ////////////////////////////////////////////////
    std::cout << "\n[INFO] [II]: Pobieranie kolejnych stron..." << std::endl;

    while (hasMorePosts && currentPage <= maxPagesToFetch){
      std::cout << "[INFO] Pobieranie strony " << currentPage << " z API zewnętrznego..." << std::endl;

      std::string jsonResponse = fetchMoreHTML(ajaxUrl, currentPage, "https://netguardia.com", sourceLink.url, "netguardia", sourceLink.url);

      try{
        json responseObj = json::parse(jsonResponse);

        if(responseObj.contains("success") && responseObj["success"] == true){
          std::string extractedHtml = responseObj["data"]["content"];
          extractedHtml = extractedHtml + "<title>" + linkExtraction(sourceLink.url, "category/","/") + " | netguardia.com</title>";

          std::vector<Article> moreArticles = parseNetGuardia(extractedHtml, false);
          std::cout << "      -> Znaleziono nowych artykułów: " << moreArticles.size() << std::endl;

          allArticles.insert(allArticles.end(), moreArticles.begin(), moreArticles.end());

          hasMorePosts = !responseObj["data"]["posts_end"];

          if (hasMorePosts){
            currentPage++;
            std::this_thread::sleep_for(std::chrono::seconds(2));
          }
        } else {
          std::cerr << "[ERROR][EXTERNAL] Serwer API nie zwrócił success: true." << std::endl;
          break;
        }
      } catch (const json::exception& e){
          std::cerr << "[ERROR][JSON] Błąd parsowania JSON: " << e.what() << std::endl;
          break;
        }
    }

    std::cout << "=====================================================";
    std::cout << "\nZAKOŃCZONO POBIERANIE. Łącznie zebrano: " << allArticles.size() << " artykółów!" << std::endl;
    std::cout << "=====================================================\n";
  } 
  
  return allArticles;
}


int main() {
    Database db("../database/cybersecurityfeed.sqlite");
    if(!db.isConnected()) return 1;

    std::vector<Article> allArticles;

    std::vector<Source> seedData = loadSourcesFromJson("../scraper/resources/sources-list.json");
    loadSourcesToDB(db.get(), seedData);

    std::vector<Source> sources = loadSourcesFromDB(db.get());

    std::vector<Article> netguardiaArticles = getNetguardiaArticles(sources);
    allArticles.insert(allArticles.end(), netguardiaArticles.begin(), netguardiaArticles.end());

    std::cout << "=====================================================";
    std::cout << "\nZAKOŃCZONO POBIERANIE. Łącznie zebrano: " << allArticles.size() << " artykółów!" << std::endl;
    std::cout << "=====================================================\n";
    //////////////////////FOR DEBUG////////////////////////////////////////
    for(size_t i = 0; i < allArticles.size(); i++){
      std::cout << "-- Tytuł: " << allArticles[i].title << std::endl;
      std::cout << "-- Link: " << allArticles[i].link << std::endl;
      std::cout << "-- Foto: " << allArticles[i].img << std::endl;
      std::cout << "------------------------------------------" << std::endl;
    }
    

    std::cout << "\n[INFO] Rozpoczynam zapis do bazy danych..." << std::endl;
    saveArticlesToDatabse(db.get(), allArticles, 1);
    return 0;
}
