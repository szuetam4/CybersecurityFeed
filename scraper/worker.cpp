#include <iostream>
#include <string>
#include <sqlite3.h>
#include <curl/curl.h>
#include <vector>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

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

struct Article {
    std::string img;
    std::string link;
    std::string title;
};

std::string linkExtraction(std::string& extractedContent, std::string& patternStart, std::string& patternEnd, size_t position){
    size_t startPos = extractedContent.find(patternStart, position);
    startPos += patternStart.length();
    size_t endPos = extractedContent.find(patternEnd, startPos);
     
    size_t contentLength = endPos - startPos;
    std::string extractedLink = extractedContent.substr(startPos, contentLength);
    return extractedLink;
}

std::string fetchMoreHTML(const std::string& apiUrl, int pageNumber) {
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
    std::string payloadSuffix = "&posts_per_page=10&query_data=%7B%22first_post_count%22%3A10%2C%22infinite_load%22%3Atrue%2C%22query_vars%22%3A%7B%22category_name%22%3A%22privacy%22%2C%22error%22%3A%22%22%2C%22m%22%3A%22%22%2C%22p%22%3A0%2C%22post_parent%22%3A%22%22%2C%22subpost%22%3A%22%22%2C%22subpost_id%22%3A%22%22%2C%22attachment%22%3A%22%22%2C%22attachment_id%22%3A0%2C%22name%22%3A%22%22%2C%22pagename%22%3A%22%22%2C%22page_id%22%3A0%2C%22second%22%3A%22%22%2C%22minute%22%3A%22%22%2C%22hour%22%3A%22%22%2C%22day%22%3A0%2C%22monthnum%22%3A0%2C%22year%22%3A0%2C%22w%22%3A0%2C%22tag%22%3A%22%22%2C%22cat%22%3A40%2C%22tag_id%22%3A%22%22%2C%22author%22%3A%22%22%2C%22author_name%22%3A%22%22%2C%22feed%22%3A%22%22%2C%22tb%22%3A%22%22%2C%22paged%22%3A0%2C%22meta_key%22%3A%22%22%2C%22meta_value%22%3A%22%22%2C%22preview%22%3A%22%22%2C%22s%22%3A%22%22%2C%22sentence%22%3A%22%22%2C%22title%22%3A%22%22%2C%22fields%22%3A%22all%22%2C%22menu_order%22%3A%22%22%2C%22embed%22%3A%22%22%2C%22category__in%22%3A%5B%5D%2C%22category__not_in%22%3A%5B%5D%2C%22category__and%22%3A%5B%5D%2C%22post__in%22%3A%5B%5D%2C%22post__not_in%22%3A%5B%5D%2C%22post_name__in%22%3A%5B%5D%2C%22tag__in%22%3A%5B%5D%2C%22tag__not_in%22%3A%5B%5D%2C%22tag__and%22%3A%5B%5D%2C%22tag_slug__in%22%3A%5B%5D%2C%22tag_slug__and%22%3A%5B%5D%2C%22post_parent__in%22%3A%5B%5D%2C%22post_parent__not_in%22%3A%5B%5D%2C%22author__in%22%3A%5B%5D%2C%22author__not_in%22%3A%5B%5D%2C%22search_columns%22%3A%5B%5D%2C%22ignore_sticky_posts%22%3Afalse%2C%22suppress_filters%22%3Afalse%2C%22cache_results%22%3Atrue%2C%22update_post_term_cache%22%3Atrue%2C%22update_menu_item_cache%22%3Afalse%2C%22lazy_load_term_meta%22%3Atrue%2C%22update_post_meta_cache%22%3Atrue%2C%22post_type%22%3A%22%22%2C%22posts_per_page%22%3A10%2C%22nopaging%22%3Afalse%2C%22comments_per_page%22%3A%2250%22%2C%22no_found_rows%22%3Afalse%2C%22order%22%3A%22DESC%22%7D%2C%22in_the_loop%22%3Afalse%2C%22is_single%22%3Afalse%2C%22is_page%22%3Afalse%2C%22is_archive%22%3Atrue%2C%22is_author%22%3Afalse%2C%22is_category%22%3Atrue%2C%22is_tag%22%3Afalse%2C%22is_tax%22%3Afalse%2C%22is_home%22%3Afalse%2C%22is_singular%22%3Afalse%7D&attributes=undefined&options=%7B%22location%22%3A%22archive%22%2C%22meta%22%3A%22archive_post_meta%22%2C%22layout%22%3A%22grid%22%2C%22columns%22%3A%224%22%2C%22image_orientation%22%3A%22landscape-16-9%22%2C%22image_size%22%3A%22csco-medium%22%2C%22summary_type%22%3A%22summary%22%2C%22excerpt%22%3Afalse%7D&_ajax_nonce=78b7189e87"; 
    
    std::string postData = payloadPrefix + std::to_string(pageNumber) + payloadSuffix;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Origin: https://netguardia.com");
    headers = curl_slist_append(headers, "Referer: https://netguardia.com/category/privacy");
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
std::vector<Article> parseNetGuardia(const std::string& htmlContent) {
    std::vector<Article> articles;

    std::string startTag = "<article class=";
    std::string endTag = "</article>";
  
    std::string imgPatternStart = "src=\"";
    std::string imgPatternEnd = "\"";
   
    std::string linkPatternStart = "href=\"";
    std::string linkPatternEnd = "/\"";
    
    std::string titlePatternStart = "title=\"";
    std::string titlePatternEnd = "\">";

    size_t searchPosition = 0;
    size_t searchImgPosition = 0;
    size_t searchLinkPosition = 0;
    size_t searchTitlePosition = 0;

    while(true){
      size_t startPos = htmlContent.find(startTag, searchPosition);
  	  if(startPos == std::string::npos){break;}

	    startPos += startTag.length();

	    size_t endPos = htmlContent.find(endTag, startPos);
	    if (endPos == std::string::npos){break;}

      size_t contentLength = endPos - startPos;
	    std::string extractedContent = htmlContent.substr(startPos, contentLength);

      std::string imgLink = linkExtraction(extractedContent, imgPatternStart, imgPatternEnd, searchImgPosition);
      std::string link = linkExtraction(extractedContent, linkPatternStart, linkPatternEnd, searchLinkPosition);
      std::string title = linkExtraction(extractedContent, titlePatternStart, titlePatternEnd, searchTitlePosition);

	    Article article;
	    article.img = imgLink;
	    article.link = link;
      article.title = title;

	    articles.push_back(article);

	    searchPosition = endPos + endTag.length();
    }
    return articles;
}
void saveArticlesToDatabse(sqlite3* db, const std::vector<Article>& articles){
  if(!db){
    std::cerr << "[ERROR][DB] Brak połączenia z bazą podczas zapisu!" << std::endl;
    return;
  }

  sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

  const char* sql = "INSERT INTO articles (title, link, img_url, published_at, source_id) VALUES (?, ?, ?, datetime('now', 'localtime'), 1);";
  sqlite3_stmt* stmt;

  if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
    std::cerr << "[ERROR][SQL] Błąd przygotowania SQL: " << sqlite3_errmsg(db) << std::endl;
    return;
  }

  int insertedCount = 0;

  for(const auto& article : articles){
    sqlite3_bind_text(stmt, 1, article.title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, article.link.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, article.img.c_str(), -1, SQLITE_STATIC);

    if(sqlite3_step(stmt) == SQLITE_DONE) {
      if(sqlite3_changes(db) > 0){
        insertedCount++;
      }
    } else {
      std::cerr << "[WARNING] Problem z artykułem: " << article.title
        << "\n[CAUSE]: " << sqlite3_errmsg(db) << "\n" << std::endl;
    }

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
  }

  sqlite3_finalize(stmt);
  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

  std::cout << "\n========================================" << std::endl;
  std::cout << "[DB] Baza zaktualizowana pomyślnie!" << std::endl;
  std::cout << "[DB] Dodano NOWYCH artykułów: " << insertedCount << std::endl;
  std::cout << "[DB] Zignorowano starych: " << (articles.size() - insertedCount) << std::endl;
  std::cout << "========================================\n" << std::endl;
}

int main() {
    Database db("../database/cybersecurityfeed.sqlite");
    if(!db.isConnected()) return 1;

    std::vector<Article> allArticles;
   
    std::string mainURL = "https://netguardia.com/category/privacy/";
    std::cout << "\n[INFO] [I]: Pobieranie kodu HTML z " << mainURL << "..." << std::endl;

    std::string mainHtml = fetchHTML(mainURL);
    std::cout << "[SUCCESS][CURL] Pobrano HTML'a o długości: " << mainHtml.length() << " znaków." << std::endl;
    
    std::vector<Article> mainArticles = parseNetGuardia(mainHtml); 
    std::cout << "[SUCCESS] Znaleziono " << mainArticles.size() << " artykułów na netguardii" << std::endl;

    allArticles.insert(allArticles.end(), mainArticles.begin(), mainArticles.end());

    std::string ajaxUrl = "https://netguardia.com/wp-json/csco/v1/more-posts";
    int currentPage = 2;
    bool hasMorePosts = true;
    int maxPagesToFetch = 5; //HARDLIMITER////////////////////////////////////////////
    std::cout << "\n[INFO] [II]: Pobieranie kolejnych stron..." << std::endl;

    while (hasMorePosts && currentPage <= maxPagesToFetch){
      std::cout << "[INFO] Pobieranie strony " << currentPage << " z API zewnętrznego..." << std::endl;

      std::string jsonResponse = fetchMoreHTML(ajaxUrl, currentPage);

      try{
        json responseObj = json::parse(jsonResponse);

        if(responseObj.contains("success") && responseObj["success"] == true){
          std::string extractedHtml = responseObj["data"]["content"];

          std::vector<Article> moreArticles = parseNetGuardia(extractedHtml);
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
    
    for(size_t i = 0; i < allArticles.size(); i++){
      std::cout << "-- Tytuł: " << allArticles[i].title << std::endl;
      std::cout << "-- Link: " << allArticles[i].link << std::endl;
      std::cout << "-- Foto: " << allArticles[i].img << std::endl;
      std::cout << "------------------------------------------" << std::endl;
    }

    std::cout << "\n[INFO] Rozpoczynam zapis do bazy danych..." << std::endl;
    saveArticlesToDatabse(db.get(), allArticles);
    return 0;
}


//Teraz musisz dopisać jeszcze połaczenie z bazą danych aby program wgrywał artykuły od bazy
//////1. zmień bazę aby można było dodawać do niej zdjęcia.
//////2. zapytaj AI czy dlaczego title, jest bez urlencoded a po curlu tytuły są encoded i jak to można naprawić
//////3. dorób zapis całęgo articles do bazy danych
//////4. zbadaj i poszerz scrapera nie tylko na /privacy ale na inne zakładki, sprawdź czy mają takie reguły <article> czynie//Każda nowa kategoria ma inny request form - dokładnie zmienia się category name. Rozważ to w programie
