<?php

$uri = file_get_contents('.uri');

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

$offLevel = 250;

if (php_sapi_name() == "cli") {
    $times = [
        "06:00:00",
        "17:00:00",
    ];
    $now = new DateTime();
    $lock = null;
    foreach ($times as $time) {
        $date = DateTime::createFromFormat("H:i:s", $time);
        if ($date->format('H') == $now->format('H') && $date->format('i') == $now->format('i')) {
            $_GET["wb"] = "tn";
            $_GET["key"] = "secret";
            $_GET["v"] = "t";
            break;
        }
    }
}

if (!getSet("s")) {
    getSet("s", "f");
}

$get = $_GET;

$secret = $get["key"] ?? "";

$forced = $get["f"] ?? 0;
if (!empty($forced)) {
    $forced = 1;
}

$response = [];

$headers = function_exists('getallheaders') ? getallheaders() : null;

$isJson = false;

$config = [
    "sender" => [
        "td" => 200,
        "fd" => 60,
    ],
    "asker"  => [
        "m" => 40,
    ]
];

if ($secret == "secret") {

    $lock = acquireLock("locked");

    if ($headers) {
        if ($headers['Accept'] === "application/json") {
            header('Content-Type: application/json');
            $get = array_merge($get, json_decode(file_get_contents("php://input"), true) ?? []);
            $isJson = true;
        } else {
            header('Content-Type: text/plain');
        }
    }

    $iam = $get["iam"] ?? null;
    $r = $get["r"] ?? null;
    $p = $get["p"] ?? null;

    if ($p && $iam) {
        $millis = getSet($iam . '.millis');
        if (empty($millis)) {
            getSet($iam . '.millis', -1);
        }
        $millis = getSet($iam . '.millis');
        if ($millis != $p) {
            getSet($iam . '.millis', $p);
            getSet($iam, date('Y-m-d H:i:s'));
        }
    }

    $webEvent = $get["wb"] ?? null;

    $key = "d" . date("y-m-d");
    $keyTime = "h" . date("H-i-s");
    if (!getSet($key)) {
        getSet($key, json_encode([]));
    }
    $data = json_decode(getSet($key), true);

    $response["wb"] = $webEvent;

    switch ($webEvent) {
        case "gc":
            $response = array_merge($response, $config[$get["v"]]);
            break;
        case "tn":
            $value = $get["v"] ?? null;
            $d = $get["d"] ?? -1;
            $w = $get["w"] ?? null;
            $wa = intval($get["wa"]) ?? 0;
            if ($w) {
                getSet('wa', $wa);
                if ($wa > $offLevel) {
                    $value = "f";
                }
            }
            if ($d > -1) {
                getSet("d", $d);
            }
            if ($forced && $d < 0) {
                $d = 0;
            }
            if ($value == "t" && ($d > 1 || $wa || $forced || php_sapi_name() == "cli")) {
                $response["s"] = "t";
                getSet("s", "t");
                $keys = array_keys($data);
                $canAddOn = true;
                if (count($keys) > 0) {
                    $last = end($keys);
                    if ($data[$last]["action"] == 1) {
                        $canAddOn = false;
                    }
                }
                if ($canAddOn) {
                    $data[$keyTime] = [
                        "action" => 1,
                        "forced" => $forced,
                        "waterSensor" => $wa
                    ];
                }
            } else if($value == "f" && (($d > 1 || $wa || $forced) || php_sapi_name() == "cli")){
                $response["s"] = "f";
                getSet("s", "f");
                $keys = array_keys($data);
                $canAddOff = true;
                if (count($keys) > 0) {
                    $last = end($keys);
                    if ($data[$last]["action"] == 0) {
                        $canAddOff = false;
                    }
                }
                if ($canAddOff) {
                    $data[$keyTime] = [
                        "action" => 0,
                        "forced" => $forced,
                        "waterSensor" => $wa,
                    ];
                }
            }
            getSet($key, json_encode($data));
            break;
        case "s":
            $s = getSet("s");
            if ($get["fo"] && $s != "f") {
                $data[$keyTime] = [
                    "action" => 0,
                    "forced" => 1,
                    "waterSensor" => 0
                ];
                getSet($key, json_encode($data));
                getSet("s", "f");
            }
            $response["s"] = getSet("s");
            if (!$iam) {
                $response["wa"] = getSet("wa");
                $response["sender"] = getSet("sender");
                $response["asker"] = getSet("asker");
            }
            break;
        case "lr":
            $response["s"] = getSet("s");
            if (!$iam) {
                $response["wa"] = getSet("wa");
                $response["sender"] = getSet("sender");
                $response["asker"] = getSet("asker");
            }
            $key = "d" . date("y-m-d");
            $data = json_decode(getSet($key), true) ?? [];
            if ($get["lk"] == "none") {
                $response["records"] = $data;
                break;
            }
            $toSend = [];
            $next = false;
            foreach ($data as $k => $record) {
                if ($get["lk"] == $k) {
                    $next = true;
                    continue;
                }
                if ($next) {
                    $toSend[$k] = $record;
                }
            }
            $response["records"] = $toSend;
            break;
        default:
            break;
    }
}

if ($secret == "secret") {
    releaseLock("locked", $lock);
    if ($isJson) {
        echo json_encode($response);
        return;
    }
    echo http_build_query($response);
    return;
}

?>

<html lang="en">
<head>
    <title>Water Control</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" type="text/css"
          href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/css/bootstrap.min.css"/>
    <link rel="stylesheet" type="text/css"
          href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.6.0/css/all.min.css"/>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Open+Sans:ital,wght@0,300..800;1,300..800&family=Roboto:ital,wght@0,100;0,300;0,400;0,500;0,700;0,900;1,100;1,300;1,400;1,500;1,700;1,900&display=swap"
          rel="stylesheet">
    <style>
        * {
            font-family: "Open Sans", sans-serif;
        }

        #main {
            padding-top: 25px;
            padding-bottom: 25px;
        }

        #force {
            margin-left: auto;
        }

        #force i {
            display: inline-block;
            margin-right: 10px;
        }

        #force.t {
            background: #2bb14a !important;
            border-color: #2bb14a !important;
        }

        #force.f {
            background: #b12b2b !important;
            border-color: #b12b2b !important;
        }

        @media (max-width: 991px) {
            #main .title {
                display: block !important;
            }

            #force {
                margin: 15px 0;
            }
        }

        table {
            margin-top: 20px;
        }

        .info p {
            margin: 0 5px;
            font-size: 13px;
        }

        .info p span {
            color: #007dd6;
        }

    </style>
</head>
<body>
<div id="main" class="container">
    <div class="title d-flex justify-content-between align-items-center">
        <h1>Water Control</h1>
        <button id="force" class="btn btn-danger color-white">Forçar</button>
    </div>
    <h3>Sensibilidade de água detectada pelo o sensor: <span id="waterSensor"><?= (getSet("wa")) ?></span></h3>
    <table id="results" class="table table-striped">
        <thead>
        <tr>
            <th>Hora</th>
            <th>Forçado</th>
            <!--th>Distância</th-->
            <th>Ação</th>
        </tr>
        </thead>
        <tbody>
        <?php
        $key = "d" . date("y-m-d");
        $data = json_decode(getSet($key), true) ?? [];
        foreach ($data as $time => $value) {
            $otime = explode("-", str_replace('h', '', $time));
            ?>
            <tr data-key="<?= $time ?>">
                <td><?= ($otime[0] . ':' . $otime[1] . ':' . $otime[2]) ?></td>
                <td><?= $value["forced"] ? "Sim" : "Não" ?></td>
                <!--td><?= ($value["waterSensor"] / 100) ?>m</td-->
                <td><?= $value["action"] ? "Ligado" : "Desligado" ?></td>
            </tr>
            <?php
        }
        ?>
        </tbody>
    </table>
    <div class="info">
        <p>Ping {sender}: <span id="sender"></span></p>
        <p>Ping {asker}: <span id="asker"></span></p>
    </div>
</div>
<script>
    let forceBtn = document.querySelector("#force");
    let results = document.querySelector("#results");
    let currentState = "";
    fetch('<?= $uri ?>&wb=s', {
        method: 'GET',
        headers: {
            'Content-Type': 'application/json',
            'Accept': 'application/json',
        }
    }).then(response => response.json()).then(data => {
        currentState = data.s;
        document.querySelector("#waterSensor").innerHTML = data.wa;
        if(data.sender) {
            document.querySelector("#sender").innerHTML = formatDate(new Date(data.sender));
        } else {
            document.querySelector("#sender").innerHTML = "n/a"
        }
        if (data.asker) {
            document.querySelector("#asker").innerHTML = formatDate(new Date(data.asker));
        } else {
            document.querySelector("#asker").innerHTML = "n/a"
        }
        if (data.s === "t") {
            forceBtn.innerHTML = "Forçar desligamento da bomba";
            forceBtn.classList.remove("t");
            forceBtn.classList.add("f");
        } else {
            forceBtn.innerHTML = "Forçar ligamento da bomba";
            forceBtn.classList.remove("f");
            forceBtn.classList.add("t");
        }
    });

    let canClick = true;

    forceBtn.addEventListener("click", () => {
        if (canClick) {
            canClick = false;
            forceBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i>' + forceBtn.innerHTML;
            if (currentState !== "") {
                fetch('<?= $uri ?>&wb=tn&f=1&v=' + (currentState === "t" ? "f" : "t"), {
                    method: 'GET',
                    headers: {
                        'Content-Type': 'application/json',
                        'Accept': 'application/json',
                    }
                }).then(response => response.json()).then(data => {
                    currentState = data.s;
                    setTimeout(() => {
                        canClick = true;
                        if (data.s === "t") {
                            forceBtn.innerHTML = "Forçar desligamento da bomba";
                            forceBtn.classList.remove("t");
                            forceBtn.classList.add("f");
                        } else {
                            forceBtn.innerHTML = "Forçar ligamento da bomba";
                            forceBtn.classList.remove("f");
                            forceBtn.classList.add("t");
                        }
                    }, 1500);
                });
            }
        }
    });

    function formatDate(date) {
        const year = date.getFullYear();
        const month = String(date.getMonth() + 1).padStart(2, '0');
        const day = String(date.getDate()).padStart(2, '0');
        const hours = String(date.getHours()).padStart(2, '0');
        const minutes = String(date.getMinutes()).padStart(2, '0');
        const seconds = String(date.getSeconds()).padStart(2, '0');
        return `${year}/${month}/${day} ${hours}:${minutes}:${seconds}`;
    }

    setInterval(() => {
        let lastTr = results.querySelector('tbody tr:last-child');
        fetch('<?= $uri ?>&wb=lr&lk=' + (lastTr ? lastTr.getAttribute('data-key') : "none"), {
            method: 'GET',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
            }
        }).then(response => response.json()).then(data => {
            currentState = data.s;
            if(data.sender) {
                document.querySelector("#sender").innerHTML = formatDate(new Date(data.sender));
            } else {
                document.querySelector("#sender").innerHTML = "n/a"
            }
            if (data.asker) {
                document.querySelector("#asker").innerHTML = formatDate(new Date(data.asker));
            } else {
                document.querySelector("#asker").innerHTML = "n/a"
            }
            document.querySelector("#waterSensor").innerHTML = data.wa;
            if (data.s === "t") {
                forceBtn.innerHTML = "Forçar desligamento da bomba";
                forceBtn.classList.remove("t");
                forceBtn.classList.add("f");
            } else {
                forceBtn.innerHTML = "Forçar ligamento da bomba";
                forceBtn.classList.remove("f");
                forceBtn.classList.add("t");
            }
            for (let i in data.records) {
                let c = data.records[i];
                let ntr = document.createElement("tr");
                let hour = document.createElement("td");
                let forced = document.createElement("td");
                let waterSensor = document.createElement("td");
                let action = document.createElement("td");
                let t = i.replace('h', '');
                t = t.split('-');
                hour.innerHTML = t[0] + ':' + t[1] + ':' + t[2];
                forced.innerHTML = c['forced'] ? "Sim" : "Não";
                action.innerHTML = c['action'] ? "Ligado" : "Desligado";
                // waterSensor.innerHTML = (c['waterSensor'] / 100) + 'm';
                ntr.setAttribute('data-key', i);
                ntr.append(hour);
                ntr.append(forced);
                // ntr.append(waterSensor);
                ntr.append(action);
                results.querySelector('tbody').append(ntr);
            }
        });
    }, 5000);
</script>
</body>
</html>
