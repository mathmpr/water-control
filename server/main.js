require('dotenv').config();
const express = require('express');
const {Model} = require('objection');
const knex = require('knex');
const path = require('path');
const cors = require('cors');
const cookieParser = require('cookie-parser');
const session = require('express-session');
const flash = require('connect-flash');
const aedes = require('aedes')();
const net = require('net');
const http = require('http');
const ws = require('websocket-stream');
const moment = require('moment-timezone');

const User = require('./models/User');
const EventLogs = require('./models/EventLogs');

const knexConfig = require('./knexfile');
const db = knex(knexConfig.development);

Model.knex(db);

const app = express();
const PORT = process.env.PORT || 3000;

app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));

app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json());
app.use(cors());
app.use(cookieParser());
app.use(session({
    secret: process.env.SESSION_SECRET || 'any-secret-thing',
    resave: false,
    saveUninitialized: false,
    cookie: {maxAge: 60000}
}));

app.use(flash());
app.use((req, res, next) => {
    const clientIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
    res.locals.isLocal = clientIp === '127.0.0.1' || clientIp === '::1';
    if (!res.locals.flash) {
        res.locals.flash = {
            success: req.flash('success'),
            error: req.flash('error'),
            info: req.flash('info')
        };
        next();
    }
});

app.use((req, res, next) => {
    res.previous = req.get('Referer') || req.get('Referrer') || false;
    res.locals.previous = res.previous;
    next();
});

const globals = {
    askerPing: null,
    senderPing: null,
};

app.use((req, res, next) => {
    req.aedes = aedes;
    req.globals = globals;
    next();
});

const webRoutes = require('./routes/web');
const apiRoutes = require('./routes/api');

app.use('/api', apiRoutes);
app.use('/', webRoutes);

const tcpServer = net.createServer(aedes.handle);
tcpServer.listen(1883, () => {
    console.log('Broker MQTT TCP rodando em mqtt://localhost:1883');
});

const httpServer = http.createServer(app);
ws.createServer({server: httpServer}, aedes.handle);
httpServer.listen(1888, () => {
    console.log('Broker MQTT via WebSocket rodando em ws://localhost:1888');
});

const secretKey = process.env.SECRET_KEY;
const iam = process.env.IAM;

const hours = [
    "08:00", "17:00"
];

const askerConfig = "15";
const senderConfig = "300:300";

const toggleWaterTopic = "toggle/water";
const onOffWaterTopic = "on_off/water";
const getConfigTopicAsker = "get_config/asker";
const getConfigTopicSender = "get_config/sender";
const keepAliveTopic = "keep/alive";
const detectWaterTopic = "detect/water";
const incomeWaterTopic = "income/water";

const getStatusTopic = "get/status";
const onStatusTopic = "on/status";
const onKeepAliveTopic = "on/keep_alive";
const newLogAddedTopic = "new/log_added";
const lastRowWaterTopic = "new/water_income";

let waterPumpStatus = false;
let alreadySetIncome = false;
let firstActive = false;
let oldValueStatus;

async function toggleWaterFn(value, user, triggerType, income = null, detect = null) {
    value = !!value;
    if (!value) {
        alreadySetIncome = false;
        firstActive = false;
    }
    if (oldValueStatus !== value) {
        oldValueStatus = value;
    }
    waterPumpStatus = value;
    if (user.username !== 'asker') {
        aedes.publish({
            topic: toggleWaterTopic,
            payload: waterPumpStatus ? '1' : '0'
        });
    }
    console.log('Published to', toggleWaterTopic, 'the value:', waterPumpStatus ? '1' : '0');
    await addRowLog(user, value, triggerType, income, detect);
    console.log('ROW saved and sent');
    publishStatus();
}

async function publishers() {
    let user = await User.query().findOne({token: secretKey});
    const hour = moment().tz("America/Sao_Paulo").format("HH:mm");
    if (hours.includes(hour) && user) {
        await toggleWaterFn(true, user, 'auto');
    }

    /*
    const logs = await EventLogs.query()
        .where('created_at', 'between', [
            moment().subtract(15, 'days').format("YYYY-MM-DD HH:mm:ss"),
            moment().format("YYYY-MM-DD HH:mm:ss")
        ])
        .orderBy('created_at', 'desc')
        .withGraphFetched('user');
     */

    aedes.publish({
        topic: getConfigTopicAsker,
        payload: askerConfig
    });

    aedes.publish({
        topic: getConfigTopicSender,
        payload: senderConfig
    });
}

let ticks = 0;
setInterval(async () => {
    if (waterPumpStatus && !firstActive) {
        ticks = 0;
        firstActive = {
            date: new Date(),
            user: await User.query().findOne({token: secretKey})
        };
    } else if (waterPumpStatus) {
        const now = new Date();
        const diffInMinutes = Math.floor((now - firstActive.date) / 60000);
        if (diffInMinutes >= parseInt(askerConfig)) {
            ticks++;
        }
        if (ticks >= 2) {
            console.log("Auto turn off after", diffInMinutes, "minutes");
            await toggleWaterFn(false, firstActive.user, 'auto');
        }
    }
}, 1000);

function beginInterval() {
    setInterval(async () => {
        await publishers();
    }, 60000);
}

let startInterval = setInterval(async () => {
    const second = moment().tz("America/Sao_Paulo").format("ss");
    if (second === "00") {
        clearInterval(startInterval);
        await publishers();
        beginInterval();
    }
}, 1000);

let allowed = {};

function publishStatus() {
    aedes.publish({
        topic: onStatusTopic,
        payload: JSON.stringify({
            status: waterPumpStatus,
            askerPing: globals.askerPing ? globals.askerPing : null,
            senderPing: globals.senderPing ? globals.senderPing : null,
        })
    })
}

async function addRowLog(user, value, triggerType, income, detect) {
    let log = {
        user_id: user.id,
        value: !!value,
        trigger_type: triggerType,
        income: income ? parseInt(income) : 0,
        detect: detect ? parseInt(detect) : 0
    }
    await EventLogs.query().insert(log);
    log.user = {
        id: user.id,
        username: user.username,
        email: user.email,
        name: user.name
    }
    log.created_at = moment().format("YYYY-MM-DD HH:mm:ss");
    aedes.publish({
        topic: newLogAddedTopic,
        payload: JSON.stringify(log)
    })
}

aedes.on('publish', async (packet, client) => {
    if (!client) return;

    const topic = packet.topic;
    let payload = packet.payload.toString();
    let originalPayload = payload;
    payload = payload.split(':');

    const secret = payload.shift();
    if (!allowed[secret]) {
        let user = await User.query().findOne({token: secret});
        if (user) {
            allowed[secret] = user;
        }
    }
    if (!allowed[secret]) return;
    const who = payload.shift();

    if (topic === onOffWaterTopic) {
        const triggerType = payload.shift();
        const value = !!(parseInt(payload.shift()));
        console.log(oldValueStatus, value, triggerType, who);
        // avoid republish the same status for concurrent clients publishing at the same time
        if (oldValueStatus === value) {
            return;
        }
        console.log("Try to: ", !!value , " by ", allowed[secret].username, " with ", triggerType);
        await toggleWaterFn(!!value, allowed[secret], triggerType);
    } else if (topic === getStatusTopic) {
        publishStatus();
    }
    if (topic === keepAliveTopic) {
        let key = who + 'Ping';
        globals[key] = moment().tz("America/Sao_Paulo").format("HH:mm:ss");
        aedes.publish({
            topic: onKeepAliveTopic,
            payload: JSON.stringify({
                askerPing: globals.askerPing ? globals.askerPing : null,
                senderPing: globals.senderPing ? globals.senderPing : null,
            })
        });
    } else if (topic === detectWaterTopic) {
        const value = parseInt(payload.shift());
        if (waterPumpStatus) {
            await toggleWaterFn(false, allowed[secret], 'auto', null, value);
        }
    } else if (topic === incomeWaterTopic) {
        const value = parseInt(payload.shift());
        if (waterPumpStatus && !alreadySetIncome) {
            alreadySetIncome = true;
            const lastLog = await EventLogs.query()
                .andWhere('value', true)
                .andWhere('income', 0)
                .orderBy('created_at', 'desc')
                .first();
            if (lastLog) {
                await EventLogs.query().findById(lastLog.id).patch({income: value});
                aedes.publish({
                    topic: lastRowWaterTopic,
                    payload: ''
                });
            }
        }
    }
});