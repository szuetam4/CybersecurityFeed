-- database/seed.sql

-- Czyszczenie starych danych
DELETE FROM article_category;
DELETE FROM articles;
DELETE FROM categories;
DELETE FROM sources;

-- Resetowanie liczników AUTOINCREMENT (SQLite)
DELETE FROM sqlite_sequence WHERE name IN ('articles', 'sources', 'categories');

-- 1. Źródła
INSERT INTO sources (id, name, url) VALUES
(1, 'AutoKult', 'https://autokult.pl'),
(2, 'Moto.pl', 'https://moto.pl'),
(3, 'Auto Świat', 'https://autoswiat.pl'),
(4, 'AntyWeb Auto', 'https://antyweb.pl');

-- 2. Kategorie
INSERT INTO categories (id, name) VALUES
(1, 'Elektromobilność'),
(2, 'Premiery'),
(3, 'Testy i Recenzje'),
(4, 'Poradniki'),
(5, 'Sporty Motorowe');

-- 3. Artykuły (20 wierszy)
INSERT INTO articles (id, source_id, title, link, published_at) VALUES
(1, 1, 'Elektryki zdominują rynek do 2030? Nowe przepisy w UE', 'https://autokult.pl/elektryki-2030', '2026-08-14 12:30:00'),
(2, 2, 'Nowe Porsche 911 GT3 RS zaprezentowane – pierwsze zdjęcia', 'https://moto.pl/porsche-911-gt3-rs', '2026-08-14 10:15:00'),
(3, 3, 'Jak przygotować samochód na dłuższą trasę urlopową?', 'https://autoswiat.pl/poradnik-trasa', '2026-08-13 18:45:00'),
(4, 1, 'Test długodystansowy: Tesla Model Y po 50 000 km', 'https://autokult.pl/test-tesla-model-y', '2026-08-13 14:20:00'),
(5, 4, 'Czy chińskie marki opanują Europę? Raport rynkowy 2026', 'https://antyweb.pl/chinskie-auta-2026', '2026-08-13 09:00:00'),
(6, 2, 'Podsumowanie GP Monako: Szalony wyścig w deszczu', 'https://moto.pl/gp-monako-2026', '2026-08-12 21:10:00'),
(7, 3, 'Co oznacza nowa kontrolka w samochodzie? Poradnik kierowcy', 'https://autoswiat.pl/kontrolka-poradnik', '2026-08-12 16:05:00'),
(8, 1, 'BMW M5 Touring debiutuje na torze Nürburgring', 'https://autokult.pl/bmw-m5-touring', '2026-08-12 11:30:00'),
(9, 4, 'Autonomiczne taksówki w Warszawie? Ruszają testy', 'https://antyweb.pl/autonomiczne-taksowki-waw', '2026-08-11 19:40:00'),
(10, 2, 'Ceny paliw na wakacje 2026: Prognozy analityków', 'https://moto.pl/ceny-paliw-wakacje-2026', '2026-08-11 13:15:00'),
(11, 3, 'Używany kompakt do 40 tysięcy zł – co wybrać?', 'https://autoswiat.pl/uzywany-kompakt-40k', '2026-08-11 08:50:00'),
(12, 1, 'Audi Concept C – przyszłość elektrycznych crossoverów', 'https://autokult.pl/audi-concept-c', '2026-08-10 17:25:00'),
(13, 4, 'Nowe baterie ze stałym elektrolitem – przełom w EV?', 'https://antyweb.pl/baterie-staly-elektrolit', '2026-08-10 12:00:00'),
(14, 2, 'Ferrari zdradza plany na napęd hybrydowy nowej generacji', 'https://moto.pl/ferrari-hybrydy-plany', '2026-08-10 09:30:00'),
(15, 3, 'Wymiana oleju w skrzyni automatycznej – fakty i mity', 'https://autoswiat.pl/olej-skrzynia-automatyczna', '2026-08-09 15:40:00'),
(16, 1, 'Alfa Romeo Giulia 2027 złapana w kamuflażu na testach', 'https://autokult.pl/alfa-romeo-giulia-2027', '2026-08-09 11:10:00'),
(17, 4, 'Polski rynek carsharingu – czy to się jeszcze opłaca?', 'https://antyweb.pl/carsharing-polska-2026', '2026-08-08 20:00:00'),
(18, 2, 'WEC 2026: Zacięta walka na torze Spa-Francorchamps', 'https://moto.pl/wec-spa-2026', '2026-08-08 16:30:00'),
(19, 3, 'Najmniej awaryjne SUV-y ostatnich lat według TÜV', 'https://autoswiat.pl/najmniej-awaryjne-suvy', '2026-08-08 10:15:00'),
(20, 1, 'Hyundai Ioniq 6 N – koreański hothatch na prąd', 'https://autokult.pl/hyundai-ioniq-6-n', '2026-08-07 14:00:00');

-- 4. Przypisanie kategorii do artykułów (M:N)
INSERT INTO article_category (article_id, category_id) VALUES
(1, 1), (1, 2),
(2, 2), (2, 3),
(3, 4),
(4, 1), (4, 3),
(5, 1),
(6, 5),
(7, 4),
(8, 2), (8, 3),
(9, 1), (9, 4),
(10, 4),
(11, 3), (11, 4),
(12, 1), (12, 2),
(13, 1),
(14, 1), (14, 2),
(15, 4),
(16, 2),
(17, 4),
(18, 5),
(19, 3), (19, 4),
(20, 1), (20, 3);
