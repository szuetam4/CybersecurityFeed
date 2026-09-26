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

$limit = isset($_GET['limit']) ? min(max((int)$_GET['limit'], 1), 100) : 20;

$offset = isset($_GET['offset']) ? max((int)$_GET['offset'], 0) : 0;

try {
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
      $escapedSearch = str_replace(['%', '_'], ['\%', '\_'], $search);
      $conditions[] = "a.title LIKE :search ESCAPE '\\'";
      $params[':search'] = '%' . $escapedSearch . '%';
    }

    $whereClause = !empty($conditions) ? " WHERE " . implode(' AND ', $conditions) : '';

    $countQuery = "SELECT COUNT(*) FROM articles a" . $whereClause;
    $countStmt = $pdo->prepare($countQuery);
    $countStmt->execute($params);
    $total = (int)$countStmt->fetchColumn();
   
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
        $whereClause
        GROUP BY a.id, a.title, a.link, a.img_url, a.published_at, s.name 
        ORDER BY a.published_at $sort
        LIMIT :limit OFFSET :offset
        ";

    $stmt = $pdo->prepare($query);

    foreach ($params as $key => $val) {
      $stmt->bindValue($key, $val);
    }

    $stmt->bindValue(':limit', $limit, PDO::PARAM_INT);
    $stmt->bindValue(':offset', $offset, PDO::PARAM_INT);
    $stmt->execute();

    $articles = $stmt->fetchAll(PDO::FETCH_ASSOC);

    http_response_code(200);
    echo json_encode([
        'status' => 'success',
        'count' => count($articles),
        'total' => $total,
        'has_more' => ($offset + count($articles)) < $total,
        'data' => $articles
    ]);

} catch (\Throwable $e) {
    error_log('[DB] ' . $e->getMessage() . ' in ' . $e->getFile() . ':' . $e->getLine());
    http_response_code(500);
    echo json_encode([
      'status' => 'error',
      'message' => 'Wystąpił błąd serwera. Spróbuj ponownie później.'
    ]);
    exit;
}
?>
