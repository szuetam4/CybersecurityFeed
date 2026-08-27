<?php

header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=UTF-8');

require_once 'db.php';

try{
    $query = "SELECT id, name FROM categories ORDER BY name ASC";

    $stmt = $pdo->prepare($query);
    $stmt->execute();
    $categories = $stmt->fetchAll();

    http_response_code(200);
    echo json_encode([
        'status' => 'success',
        'count' => count($categories),
        'data' => $categories
    ]);

} catch (\Throwable $e) {
    http_response_code(500);
    echo json_encode([
       'error' => 'Failed to fetch categories.',
       'message' => $e->getMessage()
    ]);
}
?>
