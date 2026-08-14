# CybersecurityFeed

*A high-performance cybersecurity content aggregator built with a micro-architecture approach.*

## 📖 About The Project

CybersecurityFeed is a distributed system designed to aggregate cybersecurity news, articles, and media from various sources into one centralized hub. Instead of relying on a monolithic architecture, the system separates heavy data ingestion tasks from the API layer, ensuring high performance and scalability.

This project was built to demonstrate full-stack engineering capabilities, including background processing, API design, and asynchronous frontend integration.

## 🏗️ Architecture & Tech Stack

The system is divided into three decoupled components:

*   **Data Ingestion Worker (C++):** A background process responsible for fetching and parsing external data (HTML) efficiently without blocking the web server.
*   **REST API (PHP):** A lightweight backend that connects to the database and serves normalized data to the client via JSON endpoints.
*   **Client Interface (JS/HTML/CSS):** A dynamic Single Page Application (SPA) that consumes the API and presents the feed to the user.
*   **Database:** MySQL (Relational structure ensuring data normalization).

## ✨ MVP Requirements & Features

1.  **Automated Scraping:** The C++ worker autonomously fetches and parses data from at least 2 predefined cybersecurity sources.
2.  **RESTful Endpoints:** The PHP backend exposes a `/api/news` endpoint returning aggregated articles.
3.  **Sorting Capabilities:** Users can sort the feed by publication date (newest first / oldest first).
4.  **Responsive UI:** A clean, grid-based web interface built with vanilla JavaScript (Fetch API) and CSS.
5.  **Data Normalization:** A fully normalized relational database schema (Articles, Categories, Sources) avoiding data duplication.

## 🚀 Getting Started

*(This section will be updated with setup instructions once the core modules are implemented).*

