<?php

header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=UTF-8');
header("Content-Security-Policy: default-src 'self'; img-src *; script-src 'self'; style-src 'self'");
header("X-Content-Type-Options: nosniff");
header("X-Frame-Options: DENY");

require_once 'db.php';

$sort = (isset($_GET['sort']) && strtolower($_GET['sort']) === 'asc') ? 'ASC' : 'DESC';

$category = isset($_GET['category']) && is_string($_GET['category']) ? substr(trim($_GET['category']), 0, 50) : null;

$search = isset($_GET['q']) && is_string($_GET['q']) ? substr(trim($_GET['q']), 0, 100) : null;

try {
    $query = "
        SELECT
            a.id,
            a.title,
            a.link,
            a.img_url,
            a.published_at,
            s.name as source_name,
            GROUP_CONCAT(c.name, ', ') AS tags
        FROM articles a
        JOIN sources s ON a.source_id = s.id
        LEFT JOIN article_category ac ON a.id = ac.article_id
        LEFT JOIN categories c ON ac.category_id = c.id
    ";
    $conditions = [];
    $params = [];

    if ($category) {
        $conditions[] = "a.id IN (
            SELECT ac_sub.article_id
            FROM article_category ac_sub
            JOIN categories c_sub ON ac_sub.category_id = c_sub.id
            WHERE c_sub.name = :category
        )";
        $params[':category'] = $category;
    }

    if($search){
      $conditions[] = "a.title LIKE :search";
      $params[':search'] = '%' . $search . '%';
    }

    if(!empty($conditions)){
      $query .= " WHERE " . implode(' AND ', $conditions);
    }

    $query .= " GROUP BY a.id, a.title, a.link, a.img_url, a.published_at, s.name ORDER BY a.published_at $sort";

    $stmt = $pdo->prepare($query);
    $stmt->execute($params);

    $articles = $stmt->fetchAll(PDO::FETCH_ASSOC);

    http_response_code(200);
    echo json_encode([
        'status' => 'success',
        'count' => count($articles),
        'data' => $articles
    ]);

} catch (\Throwable $e) {
    error_log('[DB] ' . $e->getMessage() . ' in ' . $e->getFile() . ':' . $e->getLine());
    http_response_code(500);
    echo json_encode([
      'status' => 'error',
      'message' => 'Wystąpił błąd serwera. Spróbuj ponownie później.'
    ]);
}
?>
