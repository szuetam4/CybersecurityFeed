<?php

header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=UTF-8');
header("Content-Security-Policy: default-src 'self'; img-src *; script-src 'self'; style-src 'self'");
header("X-Content-Type-Options: nosniff");
header("X-Frame-Options: DENY");

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
    error_log('[DB] ' . $e->getMessage() . ' in ' . $e->getFile() . ':' . $e->getLine());
    http_response_code(500);
    echo json_encode([
      'status' => 'error',
      'message' => 'Wystąpił błąd serwera. Spróbuj ponownie później.'
    ]);
}
?>
