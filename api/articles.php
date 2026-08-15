<?php

header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=UTF-8');

require_once 'db.php';

$sort = (isset($_GET['sort']) && strtolower($_GET['sort']) === 'asc') ? 'ASC' : 'DESC';

$category = $_GET['category'] ?? null;

try {
    $query = "
        SELECT
            a.id,
            a.title,
            a.link,
            a.published_at,
            s.name as source_name,
            GROUP_CONCAT(c.name, ', ') AS tags
        FROM articles a
        JOIN sources s ON a.source_id = s.id
        LEFT JOIN article_category ac ON a.id = ac.article_id
        LEFT JOIN categories c ON ac.category_id = c.id
    ";
    $params = [];

    if ($category) {
        $query .= " WHERE a.id IN (
            SELECT ac_sub.article_id
            FROM article_category ac_sub
            JOIN categories c_sub ON ac_sub.category_id = c_sub.id
            WHERE c_sub.name = :category
        )";
        $params[':category'] = $category;
    }

    $query .= " GROUP BY a.id, a.title ORDER BY a.published_at $sort";

    $stmt = $pdo->prepare($query);
    $stmt->execute($params);

    $articles = $stmt->fetchAll();

    http_response_code(200);
    echo json_encode([
        'status' => 'success',
        'count' => count($articles),
        'data' => $articles
    ]);

} catch (\Throwable $e) {
    http_response_code(500);
    echo json_encode([
        'error' => 'Failed to fetch articles.',
        'message' => $e->getMessage()
    ]);
}
