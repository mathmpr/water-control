const User = require('../../models/User');
const EventLogs = require('../../models/EventLogs');
const {OpenAI} = require('openai');
const mqtt = require('mqtt');
const moment = require("moment-timezone");

let client = new OpenAI({
    organization: process.env.OPENAI_ORGANIZATION,
    project: process.env.OPENAI_PROJECT,
    apiKey: process.env.OPENAI_API_KEY
});

const mqttClient = mqtt.connect('mqtt://localhost:1883', {
    clientId: 'alexa',
});

const onOffWaterTopic = "on_off/water";

mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
})

function sendResponse(res, text, shouldEndSession = true) {
    res.status(200).send(JSON.stringify({
        version: '1.0',
        response: {
            outputSpeech: {
                type: 'PlainText',
                text: text
            },
            shouldEndSession: shouldEndSession
        }
    }));
}

module.exports = {
    index: async function (req, res) {

        aedes = req.aedes;

        res.type('application/json');

        if (!req.body?.session?.user?.userId) {
            res.status(401).send('Unauthorized');
            return;
        }

        let intent = req.body?.request?.intent?.name;
        let slots = req.body?.request?.intent?.slots;
        let userId = req.body.session.user.userId;

        let user = await User.query().findOne({alexa_id: userId});

        if (slots && !user) {
            let user = await User.query().findOne({alexa_2fa: slots.FullLiteral.value});
            if (user) {
                await User.query().patchAndFetchById(user.id, {alexa_id: req.body.session.user.userId});
                return sendResponse(
                    res,
                    'Tudo certo. Agora você pode dizer "quero ligar a bomba" ou "quero desligar a bomba" para controlar a bomba de água.',
                    true
                );
            } else {
                return sendResponse(
                    res,
                    'Desculpe, esse número não é válido. Verifique na tela inicial do app o número correto e tente novamente.',
                    true
                );
            }
        }

        if (!user) {
            return sendResponse(
                res,
                'Nunca estive aqui antes. Na tela inicial do app existe um número. Diga "meu número é" seguido do número que está lá para me reconhecer.',
                false
            );
        }

        if (slots) {
            let completion = await client.chat.completions.create({
                model: "gpt-4o",
                messages: [
                    {
                        role: "system",
                        content: "Você é um assistente que converte comandos de voz. Um JSON será passado, sendo uma lista de comandos 'key': 'value'. O comando de vós poderá ser igual ou muito similar a uma das keys no JSON. Encontre a key mais similar possivel e retorne apenas o valor correspondente. Se não encontrar nenhuma key similar, retorne apenas com o valor 'undefined'"
                    },
                    {
                        role: "user",
                        content: `Comando de voz: "${slots.FullLiteral.value}\nJson: {"ligar a bomba": "on", "desligar a bomba": "off"}`
                    }
                ]
            });

            let command = completion.choices[0].message.content.trim().toLowerCase();

            if (['on', 'off'].includes(command)) {
                mqttClient.publish(onOffWaterTopic, `${user.token}:${user.username}:alexa:${(command === 'on' ? '1' : '0')}`);
            }
            return sendResponse(
                res,
                command === 'on' ? 'Ligando a bomba de água.' : command === 'off' ? 'Desligando a bomba de água.' : 'Desculpe, não entendi o comando. Diga "ligar a bomba" ou "desligar a bomba".',
                true
            );
        } else {

            let dayStart = moment().tz("America/Sao_Paulo").startOf('day');
            dayStart.add(3, 'hours');
            let fromDate = dayStart.format("YYYY-MM-DD HH:mm:ss");
            dayStart.add(24, 'hours').subtract(1, 'seconds');
            let toDate = dayStart.format("YYYY-MM-DD HH:mm:ss");

            const logs = await EventLogs.query()
                .where('created_at', 'between', [`${fromDate}`, `${toDate}`])
                .orderBy('created_at', 'desc')
                .limit(2)
                .withGraphFetched('user');

            let message = 'Nada a dizer por enquanto.';

            if (logs.length === 0) {
                message = 'O sistema ainda não foi ligado hoje.';
            }

            if (logs.length === 1) {
                let log = logs[0];
                if (log.value) {
                    message = 'A bomba de água está ligada desde às ' + moment(log.created_at).tz("America/Sao_Paulo").format("HH:mm");
                }
                if (log.income > 0) {
                    message += ' e água chegou até a caixa d\'água.';
                }
                if (!log.value) {
                    message = 'A bomba de água foi desligada às ' + moment(log.created_at).tz("America/Sao_Paulo").format("HH:mm");
                }
            }

            if (logs.length === 2) {
                let off = logs[0];
                let on = logs[1];

                if (off.value) {
                    message = 'A bomba de água está ligada desde às ' + moment(off.created_at).tz("America/Sao_Paulo").format("HH:mm");
                    if (off.income > 0) {
                        message += ' e água chegou até a caixa d\'água.';
                    }
                } else {
                    if (on.value && !off.value) {
                        if (off.detect > 0 || ['online', 'alexa', 'manual'].includes(off.trigger_type)) {
                            // diff in minutes and seconds between on and off
                            let duration = moment.duration(moment(off.created_at).diff(moment(on.created_at)));
                            let hours = Math.floor(duration.asHours());
                            let minutes = duration.minutes();
                            let seconds = duration.seconds();
                            message = 'O sistema funcionou normalmente durante ' + (hours > 0 ? hours + ' hora' + (hours > 1 ? 's' : '') +', ' : '') + (minutes > 0 ? minutes + ` minuto${minutes > 1 ? 's' : ''} e ` : '') + seconds + ' segundo' + (seconds > 1 ? 's' : '') + '.';
                        }
                        if (off.trigger_type === 'auto' && off.detect < 1 && on.income > 0) {
                            message = 'O sistema funcionou, mas não por tempo suficiente para encher a caixa d\'água.';
                        }
                        if (off.trigger_type === 'auto' && off.detect < 1 && on.income < 1) {
                            message = 'Nenhuma água chegou até a caixa d\'água.';
                        }
                    }
                }
            }

            return sendResponse(
                res,
                message,
                false
            );
        }
    }
}