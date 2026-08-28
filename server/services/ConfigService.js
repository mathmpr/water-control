const GeneralConfig = require('../models/GeneralConfig');

const defaults = {
    asker: '15',
    sender: '300:300',
    hours: '08:00,17:00',
};

const keys = Object.keys(defaults);

function normalizeTime(value) {
    const match = String(value).trim().match(/^(\d{1,2}):(\d{2})$/);

    if (!match) {
        return null;
    }

    const hour = Number(match[1]);
    const minute = Number(match[2]);

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return null;
    }

    return `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`;
}

function parseHours(value) {
    if (Array.isArray(value)) {
        return value.map(normalizeTime).filter(Boolean);
    }

    return String(value ?? '')
        .split(/[\s,;]+/)
        .map(normalizeTime)
        .filter(Boolean);
}

function normalizePayload(payload) {
    const askerTimeout = Number(payload.asker);
    const incomeSensitivity = Number(payload.sender_income);
    const topSensitivity = Number(payload.sender_top);
    const hours = parseHours(payload.hours);

    if (!Number.isInteger(askerTimeout) || askerTimeout < 1 || askerTimeout > 240) {
        throw new Error('O timeout do relé deve ser um número inteiro entre 1 e 240 minutos.');
    }

    if (!Number.isInteger(incomeSensitivity) || incomeSensitivity < 1 || incomeSensitivity > 4095) {
        throw new Error('A sensibilidade do sensor de chegada deve ser um número inteiro entre 1 e 4095.');
    }

    if (!Number.isInteger(topSensitivity) || topSensitivity < 1 || topSensitivity > 4095) {
        throw new Error('A sensibilidade do sensor do topo deve ser um número inteiro entre 1 e 4095.');
    }

    if (!hours.length) {
        throw new Error('Informe pelo menos um horário diário no formato HH:mm.');
    }

    return {
        asker: String(askerTimeout),
        sender: `${incomeSensitivity}:${topSensitivity}`,
        hours: [...new Set(hours)].sort().join(','),
    };
}

function presentConfig(rows) {
    const raw = {...defaults};

    for (const row of rows) {
        if (keys.includes(row.config)) {
            raw[row.config] = row.value;
        }
    }

    const [senderIncome, senderTop] = raw.sender.split(':');
    const hours = parseHours(raw.hours);

    return {
        asker: Number(raw.asker),
        sender: raw.sender,
        sender_income: Number(senderIncome),
        sender_top: Number(senderTop),
        hours,
        hours_text: hours.join(', '),
        raw,
    };
}

async function ensureDefaults() {
    for (const [config, value] of Object.entries(defaults)) {
        const existing = await GeneralConfig.query().findOne({config});

        if (!existing) {
            await GeneralConfig.query().insert({config, value});
        }
    }
}

async function getConfig() {
    await ensureDefaults();

    const rows = await GeneralConfig.query().whereIn('config', keys);

    return presentConfig(rows);
}

async function saveConfig(payload) {
    const normalized = normalizePayload(payload);

    for (const [config, value] of Object.entries(normalized)) {
        const existing = await GeneralConfig.query().findOne({config});

        if (existing) {
            await GeneralConfig.query().patchAndFetchById(existing.id, {value});
        } else {
            await GeneralConfig.query().insert({config, value});
        }
    }

    return getConfig();
}

module.exports = {
    defaults,
    getConfig,
    normalizePayload,
    parseHours,
    saveConfig,
};
