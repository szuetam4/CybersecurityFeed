#include <iostream>
#include <string>
#include <sqlite3.h>
#include <curl/curl.h>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <pugixml.hpp>
#include <ctime>

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
    std::vector<std::string> tags; // artykuł może mieć wiele kategorii (np. THN daje zwykle dwie)
    std::string publishedAt;
    int sourceId;
};

struct Source {
  std::string name;
  std::string url;
  std::string type = "html_scraper";
  int sourceId = 0;
};

std::string getEnvOrDefault(const char* name, const std::string& defaultValue){
  const char* val = std::getenv(name);
  return val ? std::string(val) : defaultValue;
}
int getEnvOrDefault(const char* name, const int defaultValue){
  const char* val = std::getenv(name);
  if(!val) return defaultValue;

  try{
    return std::stoi(val);
  } catch (const std::exception&){
    std::cerr << "[WARNING] Nieprawidłowa wartość dla " << name << " (\"" << val << "\"), używa domyślnej: " << defaultValue << std::endl;
    return defaultValue;
  }
}

void loadSourcesToDB(sqlite3* db, const std::vector<Source>& initialSources){
  if(!db) return;

  const char* sql = "INSERT OR IGNORE INTO sources (name, url, type) VALUES (?, ?, ?);";
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
    sqlite3_bind_text(stmt, 3, src.type.c_str(), -1, SQLITE_TRANSIENT);

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

  const char* sql = "SELECT id, name, url, type FROM sources;";
  sqlite3_stmt* stmt;

  if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
    std::cerr << "[ERROR] Błąd wczytywania źródeł: " << sqlite3_errmsg(db) << std::endl;
    return sources;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW){
    Source src;
    src.sourceId = sqlite3_column_int(stmt, 0);
    src.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    src.url  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const unsigned char* typeText = sqlite3_column_text(stmt, 3);
    src.type = typeText ? reinterpret_cast<const char*>(typeText) : "html_scraper";

    sources.push_back(src);
  }
  sqlite3_finalize(stmt);
  std::cout << "[INFO][DB] Pobrano " << sources.size() << " żródeł do scrapowania." << std::endl;

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
    json jsonData;
    file >> jsonData;
    for(const auto& item : jsonData["websites"]){
      Source src;
      src.name = item["name"].get<std::string>();
      src.url  = item["url"].get<std::string>();
      src.type = item.value("type", "html_scraper");
      sources.push_back(src);
    }
  } catch (const json::exception& e){
    std::cerr << "[ERROR][JSON] Bład parsowania źródeł: " << e.what() << std::endl;
  }
  return sources;
}

// Zwraca id istniejącego źródła dla danego hosta albo tworzy nowe (type='referenced').
// Używane dla linków wyciąganych z Weekendowej Lektury, gdzie host != źródło RSS.
int getOrCreateSourceIdForHost(sqlite3* db, const std::string& hostname) {
    if (!db || hostname.empty()) return 0;

    std::string sourceUrl = "https://" + hostname + "/";

    const char* insertSql = "INSERT OR IGNORE INTO sources (name, url, type) VALUES (?, ?, 'referenced');";
    sqlite3_stmt* insertStmt = nullptr;
    if (sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(insertStmt, 1, hostname.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt, 2, sourceUrl.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(insertStmt);
    } else {
        std::cerr << "[ERROR][SQL] Błąd przygotowania insertu źródła dla hosta " << hostname << ": " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(insertStmt);

    const char* selectSql = "SELECT id FROM sources WHERE url = ?;";
    sqlite3_stmt* selectStmt = nullptr;
    int sourceId = 0;
    if (sqlite3_prepare_v2(db, selectSql, -1, &selectStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(selectStmt, 1, sourceUrl.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(selectStmt) == SQLITE_ROW) {
            sourceId = sqlite3_column_int(selectStmt, 0);
        }
    } else {
        std::cerr << "[ERROR][SQL] Błąd przygotowania selectu źródła dla hosta " << hostname << ": " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(selectStmt);

    if (sourceId == 0) {
        std::cerr << "[WARNING] Nie udało się ustalić/utworzyć źródła dla hosta: " << hostname << std::endl;
    }

    return sourceId;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp){
    size_t realSize = size * nmemb;
    std::string* mem = static_cast<std::string*>(userp);
    mem->append(static_cast<char*>(contents), realSize);
    return realSize;
}

bool isFetchableUrl(const std::string& url){
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}
std::string fetchHTML(const std::string& url, const std::string& userAgent, long* outHttpCode = nullptr) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl){
        char errorBuffer[CURL_ERROR_SIZE] = {0};

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

        bool verbose = getEnvOrDefault("CURL_VERBOSE", 0) != 0;
        if (verbose) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }

        res = curl_easy_perform(curl);

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        if (outHttpCode) *outHttpCode = httpCode;

        if(res != CURLE_OK) {
            std::cerr << "[ERROR][CURL] curl_easy_perform() failed dla " << url
                       << ": " << curl_easy_strerror(res)
                       << " (" << errorBuffer << ")" << std::endl;
        } else if (httpCode >= 400) {
            std::cerr << "[WARNING][CURL] " << url << " zwrócił kod HTTP " << httpCode
                       << ", rozmiar odpowiedzi: " << readBuffer.size() << " bajtów" << std::endl;
        } else {
            std::cout << "[INFO][CURL] " << url << " -> HTTP " << httpCode
                       << ", " << readBuffer.size() << " bajtów" << std::endl;
        }

        curl_easy_cleanup(curl);
    } else {
        std::cerr << "[ERROR][CURL] Nie udało się zainicjalizować curl_easy_init() dla " << url << std::endl;
        if (outHttpCode) *outHttpCode = 0;
    }
    return readBuffer;
}
bool isImageLinkValid(const std::string& link) {
  if(link.empty()) return false;

  // .gif dodane ze względu na miniaturki na thehackernews.com (np. GIF-y w newsach o malware)
  std::vector<std::string> validExtension = {".png", ".jpeg", ".jpg", ".webp", ".gif"};

  std::string lowerLink = link;
  std::transform(lowerLink.begin(), lowerLink.end(), lowerLink.begin(), ::tolower);

  for (const auto& ext : validExtension){
    if(lowerLink.length() >= ext.length() &&
       lowerLink.compare(lowerLink.length() - ext.length(), ext.length(), ext) == 0){
      return true;
    }
  }
  return false;
}

std::string linkExtraction(const std::string& extractedContent, const std::string& patternStart, const std::string& patternEnd){
    size_t startPos = extractedContent.find(patternStart, 0);
    if(startPos == std::string::npos) return "";
    startPos += patternStart.length();

    size_t endPos = extractedContent.find(patternEnd, startPos);
    if(endPos == std::string::npos || endPos < startPos) return "";

    return extractedContent.substr(startPos, endPos - startPos);
}

// Usuwa znaczniki HTML i normalizuje białe znaki (spacje/nowe linie -> pojedyncza spacja).
std::string stripHtmlTags(const std::string& input) {
    std::string noTags;
    noTags.reserve(input.size());
    bool insideTag = false;
    for (char c : input) {
        if (c == '<') { insideTag = true; continue; }
        if (c == '>') { insideTag = false; continue; }
        if (!insideTag) noTags += c;
    }

    std::string normalized;
    normalized.reserve(noTags.size());
    bool lastWasSpace = false;
    for (char c : noTags) {
        bool isSpace = std::isspace(static_cast<unsigned char>(c));
        if (isSpace) {
            if (!lastWasSpace && !normalized.empty()) normalized += ' ';
            lastWasSpace = true;
        } else {
            normalized += c;
            lastWasSpace = false;
        }
    }
    while (!normalized.empty() && normalized.back() == ' ') normalized.pop_back();

    return normalized;
}

// Dekoduje podstawowe encje HTML, które mogą występować w tekście wyciąganym bezpośrednio
// z surowego HTML (scraper THN, linki z Weekendowej Lektury). &nbsp;/&#160; dekodujemy na
// zwykłą spację - na thehackernews.com bywa używana zamiast zwykłej spacji w separatorze
// tagów (" /&nbsp;"), co bez dekodowania psuje split po " / ". Parsery XML (pugixml)
// dekodują encje automatycznie, więc tej funkcji NIE stosujemy do danych z RSS.
// UWAGA: &amp; musi być dekodowane jako ostatnie, żeby nie popsuć encji typu &amp;lt;
std::string decodeHtmlEntities(std::string text) {
    const std::string ampEntity = "&amp;";
    size_t pos = 0;
    while ((pos = text.find(ampEntity, pos)) != std::string::npos) {
      text.replace(pos, ampEntity.length(), "&");
      // Celowo NIE przesuwamy pos o długość zamiennika - łapie to też rzadkie
      // przypadki potrójnego kodowania (&amp;amp;) w jednym przebiegu.
    }

    static const std::vector<std::pair<std::string, std::string>> entities = {
        {"&nbsp;", " "},
        {"&#160;", " "},
        {"&quot;", "\""},
        {"&#039;", "'"},
        {"&#39;", "'"},
        {"&apos;", "'"},
        {"&lt;", "<"},
        {"&gt;", ">"},
    };

    for (const auto& [entity, replacement] : entities) {
        size_t pos = 0;
        while ((pos = text.find(entity, pos)) != std::string::npos) {
            text.replace(pos, entity.length(), replacement);
            pos += replacement.length();
        }
    }
    return text;
}

// Usuwa z tekstu encje HTML, których nie da się sensownie zdekodować na zwykły znak
// (np. glify ikon z prywatnego zakresu Unicode, wstawiane przez czcionki ikon wprost
// jako tekst - THN robi tak w polu daty przed "Sep 26, 2026"). W odróżnieniu od
// decodeHtmlEntities ta funkcja nie mapuje encji na odpowiednik, tylko je wycina.
std::string stripUnknownHtmlEntities(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    size_t pos = 0;
    while (pos < text.size()) {
        if (text[pos] == '&') {
            size_t semiPos = text.find(';', pos);
            // Encje HTML mają rozsądnie ograniczoną długość (np. &#59394; to 8 znaków) -
            // ogranicz szukanie, żeby przypadkowy '&' bez towarzyszącego ';' w pobliżu
            // nie połknął fragmentu dalszego tekstu.
            if (semiPos != std::string::npos && semiPos - pos <= 10) {
                pos = semiPos + 1;
                continue;
            }
        }
        result += text[pos];
        pos++;
    }
    return result;
}

// Wyciąga host z URL-a, np. "https://cert.pl/posts/xyz/" -> "cert.pl".
std::string extractHostname(const std::string& url) {
    std::string working = url;
    size_t schemeEnd = working.find("://");
    if (schemeEnd != std::string::npos) {
        working = working.substr(schemeEnd + 3);
    }
    size_t hostEnd = working.find_first_of("/?#:");
    std::string host = (hostEnd == std::string::npos) ? working : working.substr(0, hostEnd);

    std::transform(host.begin(), host.end(), host.begin(), ::tolower);
    return host;
}

std::string parseRfc822ToSqlite(const std::string& rfc822Date) {
    struct tm tmStruct{};
    if (strptime(rfc822Date.c_str(), "%a, %d %b %Y %H:%M:%S", &tmStruct) == nullptr) {
        std::cerr << "[WARNING] Nie udało się sparsować daty: " << rfc822Date << std::endl;
        return "";
    }
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tmStruct);
    return std::string(buffer);
}

class RssFeedParser {
public:
    std::vector<Article> parse(const std::string& rawContent, int sourceId) {
        std::vector<Article> articles;

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_string(rawContent.c_str());

        if (!result) {
            std::cerr << "[ERROR][XML] Błąd parsowania RSS: " << result.description() << std::endl;
            return articles;
        }

        pugi::xml_node channel = doc.child("rss").child("channel");
        if (!channel) {
            std::cerr << "[ERROR][RSS] Brak <channel> - to nie jest poprawny RSS 2.0?" << std::endl;
            return articles;
        }

        for (pugi::xml_node item : channel.children("item")) {
            Article article;
            // pugixml dekoduje encje HTML automatycznie podczas parsowania XML-a,
            // więc tytuł/link są już w postaci gotowej do wyświetlenia.
            article.title = item.child_value("title");
            article.link  = item.child_value("link");

            std::string rawDate = item.child_value("pubDate");
            article.publishedAt = rawDate.empty() ? "" : parseRfc822ToSqlite(rawDate);

            pugi::xml_node enclosure = item.child("enclosure");
            std::string imgUrl = enclosure.attribute("url").as_string();
            article.img = isImageLinkValid(imgUrl) ? imgUrl : "";

            pugi::xml_node categoryNode = item.child("category");
            std::string tag = categoryNode ? categoryNode.child_value() : "uncategorized";
            std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
            article.tags = { tag }; // RSS daje jedną kategorię na wpis

            article.sourceId = sourceId;
            articles.push_back(article);
        }

        std::cout << "[SUCCESS] Sparsowano " << articles.size() << " artykułów z RSS" << std::endl;
        return articles;
    }
};

// ---------------------------------------------------------------------------
// Moduł: wyciąganie linków do materiałów źródłowych z wpisów typu
// "Weekendowa Lektura" (zaufanatrzeciastrona.pl).
// ---------------------------------------------------------------------------

bool isWeekendowaLekturaDigest(const std::string& title) {
    static const std::string marker = "Weekendowa Lektura";
    return title.find(marker) != std::string::npos;
}

// Kategoria samego wpisu zbiorczego (nadpisuje to, co przypisał RSS) oraz kategoria
// materiałów wyciągniętych z jego treści - celowo różne, żeby dało się je odróżnić
// przy filtrowaniu na froncie.
const std::string DIGEST_POST_TAG = "weekendowa-lektura-źródło";
const std::string DIGEST_LINK_TAG = "weekendowa-lektura";

struct DigestLink {
    std::string title;
    std::string href;
};

std::vector<DigestLink> extractLinksFromListBlock(const std::string& olBlock) {
    std::vector<DigestLink> links;
    size_t searchPos = 0;
    const std::string liCloseTag = "</li>";

    while (true) {
        size_t liStart = olBlock.find("<li", searchPos);
        if (liStart == std::string::npos) break;

        size_t liEnd = olBlock.find(liCloseTag, liStart);
        if (liEnd == std::string::npos) break;

        std::string liBlock = olBlock.substr(liStart, liEnd - liStart);

        std::string href = decodeHtmlEntities(linkExtraction(liBlock, "href=\"", "\""));
        std::string title = decodeHtmlEntities(stripHtmlTags(liBlock));

        if (!href.empty() && !title.empty()) {
            links.push_back({title, href});
        }

        searchPos = liEnd + liCloseTag.length();
    }

    return links;
}

class WeekendowaLekturaParser {
public:
    std::vector<DigestLink> parse(const std::string& articleHtml) {
        std::vector<DigestLink> extracted;
        const std::string olOpenTag = "<ol class=\"list-number\">";
        const std::string olCloseTag = "</ol>";
        size_t searchPos = 0;

        while (true) {
            size_t olStart = articleHtml.find(olOpenTag, searchPos);
            if (olStart == std::string::npos) break;

            size_t contentStart = olStart + olOpenTag.length();
            size_t olEnd = articleHtml.find(olCloseTag, contentStart);
            if (olEnd == std::string::npos) break;

            std::string olBlock = articleHtml.substr(contentStart, olEnd - contentStart);
            std::vector<DigestLink> blockLinks = extractLinksFromListBlock(olBlock);
            extracted.insert(extracted.end(), blockLinks.begin(), blockLinks.end());

            searchPos = olEnd + olCloseTag.length();
        }

        std::cout << "[SUCCESS] Wyciągnięto " << extracted.size() << " linków z wpisu Weekendowej Lektury" << std::endl;
        return extracted;
    }
};

// ---------------------------------------------------------------------------
// Moduł: scraper HTML dla thehackernews.com. Brak pełnego RSS z listą, więc
// pobieramy stronę główną i kolejne strony paginacji ("Next Page"). Realne
// artykuły odróżniamy od natywnych reklam ("newsfeed") po tym, że link do
// artykułu jest w cudzysłowie podwójnym i wskazuje na thehackernews.com,
// podczas gdy reklamy mają href w cudzysłowie pojedynczym na thehackernews.uk.
// ---------------------------------------------------------------------------

std::string parseThnDateToSqlite(const std::string& thnDate) {
    struct tm tmStruct{};
    // THN podaje na liście tylko datę, bez godziny (np. "Sep 04, 2026") - godzinę zerujemy.
    // Artykuły z tego samego dnia mogą więc sortować się między sobą w dowolnej kolejności.
    if (strptime(thnDate.c_str(), "%b %d, %Y", &tmStruct) == nullptr) {
        std::cerr << "[WARNING] Nie udało się sparsować daty THN: " << thnDate << std::endl;
        return "";
    }
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tmStruct);
    return std::string(buffer);
}

std::vector<std::string> splitThnTags(const std::string& tagsText) {
    std::vector<std::string> tags;

    // THN czasem rozdziela tagi separatorem " /&nbsp;" (spacja + slash + encja twardej
    // spacji) zamiast zwykłej " / " - bez dekodowania encji split poniżej by tego nie
    // złapał i skleiłby dwa tagi w jeden (np. "artificial intelligence /\u00a0cloud security").
    std::string decodedText = decodeHtmlEntities(tagsText);

    size_t start = 0;
    const std::string separator = " / ";

    while (true) {
        size_t sepPos = decodedText.find(separator, start);
        std::string rawTag = (sepPos == std::string::npos)
            ? decodedText.substr(start)
            : decodedText.substr(start, sepPos - start);

        size_t first = rawTag.find_first_not_of(" \t\n\r");
        if (first != std::string::npos) {
            size_t last = rawTag.find_last_not_of(" \t\n\r");
            std::string trimmed = rawTag.substr(first, last - first + 1);
            std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), ::tolower);
            tags.push_back(trimmed);
        }

        if (sepPos == std::string::npos) break;
        start = sepPos + separator.length();
    }

    return tags;
}

class TheHackerNewsParser {
public:
    std::vector<Article> scrapeAll(const std::string& startUrl, const std::string& userAgent, int maxPages, int sourceId) {
    std::vector<Article> allArticles;
    std::string currentUrl = startUrl;
    int pageCount = 0;

    while (!currentUrl.empty() && pageCount < maxPages) {
        std::cout << "\n[INFO][THN] Pobieranie strony " << (pageCount + 1) << ": " << currentUrl << std::endl;

        long httpCode = 0;
        std::string html = fetchHTML(currentUrl, userAgent, &httpCode);

        // Cloudflare (i podobne WAF-y) potrafią blokować akurat endpoint paginacji
        // ("/search?...") osobną regułą niż strona główna, zwracając stronę wyzwania
        // JS ("Just a moment...") z kodem 403 lub 429. Nie da się tego obejść po
        // stronie curla (brak wykonania JS) - traktujemy to jako naturalny koniec
        // dostępnej zawartości, nie błąd: zachowujemy to, co już zebrano.
        if (httpCode == 403 || httpCode == 429) {
            std::cout << "[INFO][THN] Strona " << currentUrl << " zwróciła kod " << httpCode
                       << " (prawdopodobnie ochrona anty-bot). Kończę paginację, zebrane dotąd artykuły ("
                       << allArticles.size() << ") zostaną zapisane." << std::endl;
            break;
        }

        if (html.empty()) {
            std::cerr << "[ERROR][THN] Pusta odpowiedź z " << currentUrl << ", przerywam." << std::endl;
            break;
        }

        std::vector<Article> pageArticles = parsePage(html, sourceId);
        std::cout << "[SUCCESS][THN] Znaleziono " << pageArticles.size() << " artykułów na stronie." << std::endl;
        allArticles.insert(allArticles.end(), pageArticles.begin(), pageArticles.end());

        pageCount++;
        currentUrl = extractNextPageUrl(html);

        if (!currentUrl.empty() && pageCount < maxPages) {
            std::this_thread::sleep_for(std::chrono::seconds(5)); // uprzejmość wobec serwera
        }
    }

    return allArticles;
}
private:
    std::vector<Article> parsePage(const std::string& html, int sourceId) {
        std::vector<Article> articles;
        const std::string blockMarker = "<div class='body-post clear'>";
        size_t searchPos = 0;

        while (true) {
            size_t blockStart = html.find(blockMarker, searchPos);
            if (blockStart == std::string::npos) break;

            size_t nextBlockStart = html.find(blockMarker, blockStart + blockMarker.length());
            std::string block = (nextBlockStart == std::string::npos)
                ? html.substr(blockStart)
                : html.substr(blockStart, nextBlockStart - blockStart);

            Article article;
            if (parseArticleBlock(block, sourceId, article)) {
                articles.push_back(article);
            }

            searchPos = blockStart + blockMarker.length();
        }

        return articles;
    }

    // Zwraca false, jeśli blok nie jest prawdziwym artykułem (np. wstawka reklamowa
    // "newsfeed" - ma href w innym cudzysłowie i na innej domenie).
    bool parseArticleBlock(const std::string& block, int sourceId, Article& article) {
        std::string link = linkExtraction(block, "href=\"", "\"");
        if (link.rfind("https://thehackernews.com/", 0) != 0) {
            return false;
        }

        std::string title = linkExtraction(block, "<h2 class='home-title'>", "</h2>");
        if (title.empty()) {
            return false;
        }

        article.link = decodeHtmlEntities(link);
        article.title = decodeHtmlEntities(title);

        std::string dateText = stripHtmlTags(linkExtraction(block, "<span class='h-datetime'>", "</span>"));
        // Usuń ewentualne encje HTML (np. glif ikony zegara wstawiony wprost jako tekst)
        // zanim spróbujemy sparsować datę - inaczej strptime zawsze zawiedzie.
        dateText = stripUnknownHtmlEntities(decodeHtmlEntities(dateText));
        article.publishedAt = parseThnDateToSqlite(dateText);

        std::string thumbnail = linkExtraction(block, "data-src='", "'");
        article.img = isImageLinkValid(thumbnail) ? decodeHtmlEntities(thumbnail) : "";

        std::string tagsText = stripHtmlTags(linkExtraction(block, "<span class='h-tags'>", "</span>"));
        article.tags = splitThnTags(tagsText);

        article.sourceId = sourceId;
        return true;
    }

    std::string extractNextPageUrl(const std::string& html) {
        std::string olderLinkBlock = linkExtraction(html, "<span id='blog-pager-older-link'>", "</span>");
        if (olderLinkBlock.empty()) return "";
        return decodeHtmlEntities(linkExtraction(olderLinkBlock, "href=\"", "\""));
    }
};

void saveArticlesToDatabase(sqlite3* db, const std::vector<Article>& articles){
  if(!db){
    std::cerr << "[ERROR][DB] Brak połączenia z bazą podczas zapisu!" << std::endl;
    return;
  }

  sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

  const char* sqlArticle =
    "INSERT OR IGNORE INTO articles (title, link, img_url, published_at, source_id) "
    "VALUES (?, ?, ?, COALESCE(NULLIF(?, ''), datetime('now', 'localtime')), ?);";
  const char* sqlTagInsert = "INSERT OR IGNORE INTO categories (name) VALUES (?);";
  const char* sqlTagSelect = "SELECT id FROM categories WHERE name = ?;";
  // OR IGNORE dodane ze względu na wiele kategorii na artykuł - zabezpieczenie przed
  // naruszeniem PRIMARY KEY(article_id, category_id), gdyby ten sam tag pojawił się dwukrotnie.
  const char* sqlBridge = "INSERT OR IGNORE INTO article_category (article_id, category_id) VALUES (?, ?);";

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
    std::vector<std::string> tags = article.tags.empty()
      ? std::vector<std::string>{"uncategorized"}
      : article.tags;

    sqlite3_bind_text(stmtArticle, 1, article.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtArticle, 2, article.link.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtArticle, 3, article.img.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtArticle, 4, article.publishedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmtArticle, 5, article.sourceId);

    if(sqlite3_step(stmtArticle) == SQLITE_DONE) {
      if(sqlite3_changes(db) > 0){
        insertedCount++;
        int articleId = sqlite3_last_insert_rowid(db);

        // Kategorie ustalamy tylko dla realnie nowych artykułów - unikamy zbędnej
        // pracy dla duplikatów zignorowanych przez INSERT OR IGNORE powyżej.
        for(const auto& tagName : tags){
          sqlite3_bind_text(stmtTagInsert, 1, tagName.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_step(stmtTagInsert);
          sqlite3_reset(stmtTagInsert);
          sqlite3_clear_bindings(stmtTagInsert);

          int tagId = 0;
          sqlite3_bind_text(stmtTagSelect, 1, tagName.c_str(), -1, SQLITE_TRANSIENT);
          if(sqlite3_step(stmtTagSelect) == SQLITE_ROW){
            tagId = sqlite3_column_int(stmtTagSelect, 0);
          }
          sqlite3_reset(stmtTagSelect);
          sqlite3_clear_bindings(stmtTagSelect);

          if(tagId == 0){
            std::cerr << "[WARNING] Nie udało się ustalić ID dla kategorii: " << tagName << std::endl;
            continue;
          }

          sqlite3_bind_int(stmtBridge, 1, articleId);
          sqlite3_bind_int(stmtBridge, 2, tagId);
          if(sqlite3_step(stmtBridge) != SQLITE_DONE){
            std::cerr << "[WARNING] Błąd łączenia kategorii \"" << tagName << "\" z artykułem (" << article.title << "): " << sqlite3_errmsg(db) << std::endl;
          }
          sqlite3_reset(stmtBridge);
          sqlite3_clear_bindings(stmtBridge);
        }
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

int main() {
    std::string dbPath          = getEnvOrDefault("DB_PATH", "/app/database/cybersecurityfeed.sqlite");
    std::string sourcesJsonPath = getEnvOrDefault("SOURCES_JSON_PATH", "/app/scraper/resources/sources-list.json");
    std::string userAgent       = getEnvOrDefault("USER_AGENT", "");
    int maxPagesToFetch         = getEnvOrDefault("MAX_PAGES_TO_FETCH", 5);

    if (userAgent.empty()) {
        std::cerr << "[ERROR] USER_AGENT nie został ustawiony" << std::endl;
        return -1;
    }

    Database db(dbPath);
    if (!db.isConnected()) return 1;

    std::vector<Source> seedData = loadSourcesFromJson(sourcesJsonPath);
    loadSourcesToDB(db.get(), seedData);

    std::vector<Source> sources = loadSourcesFromDB(db.get());
    std::vector<Article> allArticles;

    RssFeedParser rssParser;
    TheHackerNewsParser thnParser;

    for (auto& src : sources) {
        if (src.type == "rss") {
            std::cout << "\n[INFO] Pobieranie RSS z " << src.url << "..." << std::endl;
            std::string rawXml = fetchHTML(src.url, userAgent);

            if (rawXml.empty()) {
                std::cerr << "[ERROR] Pusta odpowiedź z " << src.url << ", pomijam." << std::endl;
                continue;
            }

            std::vector<Article> articles = rssParser.parse(rawXml, src.sourceId);
            allArticles.insert(allArticles.end(), articles.begin(), articles.end());

        } else if (src.type == "thn_scraper") {
            std::cout << "\n[INFO] Scrapowanie " << src.name << " (HTML)..." << std::endl;
            std::vector<Article> articles = thnParser.scrapeAll(src.url, userAgent, maxPagesToFetch, src.sourceId);
            allArticles.insert(allArticles.end(), articles.begin(), articles.end());

        } else if (src.type == "referenced") {
            // Źródła utworzone automatycznie dla hostów linkowanych w "Weekendowej Lekturze" -
            // służą wyłącznie do przypisania source_id wyciągniętym materiałom, nigdy nie są
            // scrapowane bezpośrednio. Cichy skip, bez warninga (to oczekiwane zachowanie).
            continue;

        } else {
            std::cerr << "[WARNING] Pomijam źródło \"" << src.name
                       << "\" - nieobsługiwany typ '" << src.type << "'" << std::endl;
        }
    }

    // Wpisy "Weekendowa Lektura" dostają dedykowaną kategorię, niezależną od tego,
    // co przypisał RSS - odróżnia to zbiorczy wpis od materiałów wyciągniętych z jego treści.
    for (auto& article : allArticles) {
        if (isWeekendowaLekturaDigest(article.title)) {
            article.tags = { DIGEST_POST_TAG };
        }
    }

    // Dla każdego wpisu "Weekendowa Lektura" pobieramy pełną treść strony,
    // wyciągamy z niej linki i przypisujemy im source_id na podstawie hosta
    // docelowej domeny oraz odrębną kategorię DIGEST_LINK_TAG.
    WeekendowaLekturaParser digestParser;
    std::vector<Article> digestExtractedArticles;
    std::map<std::string, int> hostSourceIdCache;

    for (const auto& article : allArticles) {
        if (!isWeekendowaLekturaDigest(article.title)) continue;

        std::cout << "\n[INFO] Wykryto wpis 'Weekendowa Lektura': " << article.title << std::endl;

        std::string fetchUrl = decodeHtmlEntities(article.link);
        if(!isFetchableUrl(fetchUrl)){
          continue;
        }
        std::string fullHtml = fetchHTML(fetchUrl, userAgent);

        if (fullHtml.empty()) {
            std::cerr << "[WARNING] Nie udało się pobrać treści artykułu: " << fetchUrl << std::endl;
            continue;
        }

        std::vector<DigestLink> links = digestParser.parse(fullHtml);

        for (const auto& link : links) {
            std::string hostname = extractHostname(link.href);
            if (hostname.empty()) {
                std::cerr << "[WARNING] Nie udało się ustalić hosta dla linku: " << link.href << std::endl;
                continue;
            }

            int sourceId;
            auto cacheIt = hostSourceIdCache.find(hostname);
            if (cacheIt != hostSourceIdCache.end()) {
                sourceId = cacheIt->second;
            } else {
                sourceId = getOrCreateSourceIdForHost(db.get(), hostname);
                hostSourceIdCache[hostname] = sourceId;
            }

            if (sourceId == 0) continue;

            Article extractedArticle;
            extractedArticle.title = link.title;
            extractedArticle.link = link.href;
            extractedArticle.img = "";
            extractedArticle.tags = { DIGEST_LINK_TAG };
            extractedArticle.publishedAt = article.publishedAt;
            extractedArticle.sourceId = sourceId;

            digestExtractedArticles.push_back(extractedArticle);
        }
    }

    allArticles.insert(allArticles.end(), digestExtractedArticles.begin(), digestExtractedArticles.end());

    std::cout << "=====================================================";
    std::cout << "\nZAKOŃCZONO POBIERANIE. Łącznie zebrano: " << allArticles.size() << " artykułów!" << std::endl;
    std::cout << "=====================================================\n";

    std::cout << "\n[INFO] Rozpoczynam zapis do bazy danych..." << std::endl;
    saveArticlesToDatabase(db.get(), allArticles);
    return 0;
}
