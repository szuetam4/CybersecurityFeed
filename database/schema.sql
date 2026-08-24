CREATE TABLE sources (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL,
    url VARCHAR(255) NOT NULL UNIQUE
);

CREATE TABLE categories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(50) NOT NULL UNIQUE
);

CREATE TABLE articles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title VARCHAR(255) NOT NULL,
    link TEXT UNIQUE NOT NULL,
    img_url TEXT,
    published_at DATETIME NOT NULL,
    source_id INTEGER NOT NULL,
    FOREIGN KEY (source_id) REFERENCES sources(id)
);

CREATE TABLE article_category (
    article_id INTEGER NOT NULL,
    category_id INTEGER NOT NULL,
    PRIMARY KEY (article_id, category_id),
    FOREIGN KEY (article_id) REFERENCES articles(id) ON DELETE CASCADE,
    FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE CASCADE
);
