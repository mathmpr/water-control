<?php

ini_set('log_errors', '1');
ini_set('error_log', __DIR__ . '/error.log');

$input = file_get_contents('php://input');

$data = [];
if (!empty($input)) {
    try {
        $data = json_decode($input, true, 512, JSON_THROW_ON_ERROR);
    } catch (Exception $e) {
        $data = [];
    }
}

$file = 'alexa_db.json';
if (!file_exists($file)) {
    file_put_contents($file, json_encode([
        'expenses' => []
    ], JSON_PRETTY_PRINT));
}

$db = json_decode(file_get_contents($file), true);

$post = array_merge($_POST, $data);

header('Content-Type: application/json');

$newSession = $post['session']['new'] ?? $post['session'] ?? false;
$type = $post['request']['type'] ?? $post['type'] ?? '';
$intentName = $post['request']['intent']['name'] ?? $post['intentName'] ?? '';

function getSlotValue($post, $slotName): string
{
    return $post['request']['intent']['slots'][$slotName]['value'] ?? $post['body'][$slotName] ?? '';
}

function getSlots($post): array
{
    return array_keys($post['request']['intent']['slots'] ?? []);
}

global $redis;
$redis = new Redis();

function getSet(string $key = "", ?string $value = null, bool $valueAsDefault = false): null|string
{
    try {
        $interval = 500;
        $initial = 0;
        $timeOut = $interval * 5;
        global $redis;
        if ($value != null && !$valueAsDefault) {
            $redis->set($key, $value);
            return null;
        }
        while (!$redis->exists($key) && $initial < $timeOut) {
            usleep($interval);
            $initial += $interval;
        }
        if ($redis->exists($key)) {
            return $redis->get($key);
        }
        if ($valueAsDefault) {
            return $value;
        }
    } catch (Throwable $th) {

    }
    return null;
}

function acquireLock($lockKey, $timeout = 10)
{
    try {
        global $redis;
        $lockAcquired = false;
        $lockValue = uniqid();

        while (!$lockAcquired) {
            $lockAcquired = $redis->set($lockKey, $lockValue, ['nx', 'ex' => $timeout]);
            if ($lockAcquired) {
                return $lockValue;
            }
            usleep(100000);
        }
    } catch (Throwable $th) {

    }
}

function releaseLock($lockKey, $lockValue): void
{
    try {
        global $redis;
        $redis->watch($lockKey);
        if ($redis->get($lockKey) === $lockValue) {
            $redis->multi();
            $redis->del($lockKey);
            $redis->exec();
        } else {
            $redis->unwatch();
        }
    } catch (Throwable $th) {

    }
}

try {
    $redis->connect('127.0.0.1');
} catch (Throwable $th) {
    exit("Redis failed");
}

date_default_timezone_set('America/Sao_Paulo');

$key = "d" . date("y-m-d");
$keyTime = "h" . date("H-i-s");
if (!getSet($key)) {
    getSet($key, json_encode([]));
}
$data = json_decode(getSet($key), true);

$arrayKeys = array_keys($data);

$off = array_pop($data);
$on = array_pop($data);

$offKey = array_pop($arrayKeys);
$onKey = array_pop($arrayKeys);

$message = 'Nada para anunciar';

if (($on && $on['action'] == 1) && !$off) {
    $message = 'O sistema ligou e não foi desligado. Problema com o asker';
}

if (($on && $on['action'] == 1) && ($off && $off['action'] == 0)) {
    $onTime = DateTime::createFromFormat('H-i-s', trim($onKey, 'h'));
    $offTime = DateTime::createFromFormat('H-i-s', trim($offKey, 'h'));
    $diff = $onTime->diff($offTime);
    if ($diff->i > 35) {
        $message = 'O sistema ligou e desligou com tempo superior a 35 minutos';
    } else {
        $message = 'O sistema ligou e desligou normalmente';
    }
}

if ($off && $off['action'] == 1) {
    $message = 'O sistema está ligado neste momento.';
}

echo json_encode([
    'version' => '1.0',
    'response' => [
        'outputSpeech' => [
            'type' => 'PlainText',
            'text' => $message
        ],
        'shouldEndSession' => true
    ]
], JSON_PRETTY_PRINT);