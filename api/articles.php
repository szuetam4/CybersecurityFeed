<?php

header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=UTF-8');

require_once 'db.php';

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
        JOIN article_category ac ON a.id = ac.article_id
        JOIN categories c ON ac.category_id = c.id
        GROUP BY a.id, a.title
        ORDER BY a.published_at DESC
    ";
    
    $stmt = $pdo->prepare($query);
    $stmt->execute();
    
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
