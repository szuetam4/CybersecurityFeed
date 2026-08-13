INSERT INTO categories (name) VALUES ('CVE'),('Video'),('Research');
INSERT INTO sources (name,url) VALUES ('sekurak','https://sekurak.pl/'),('netguardia','https://netguardia.com/');
INSERT INTO articles VALUES (1,'test nowego IDS','https://secureconnection.pl/','2025-12-29 12:21:04.5',1),(2,'włamanie do starostwa','https://secureconnection.pl/','2026-05-23 12:13:14.5',2);
INSERT INTO article_category (article_id, category_id) VALUES (1,1),(2,2),(1,3);
