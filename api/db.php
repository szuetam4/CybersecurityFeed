<?php

$dbPath = getenv('DB_PATH') ?: __DIR__ . '/../database/cybersecurityfeed.sqlite';

try {
    $pdo = new PDO('sqlite:' . $dbPath);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $pdo->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
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
