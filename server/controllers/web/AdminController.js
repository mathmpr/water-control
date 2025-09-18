const EventLogs = require('../../models/EventLogs');
const moment = require('moment-timezone');

module.exports = {
    index: async (req, res) => {

        let dayStart = moment().tz("America/Sao_Paulo").startOf('day');
        dayStart.add(3, 'hours');
        let fromDate = dayStart.format("YYYY-MM-DD HH:mm:ss");
        dayStart.add(24, 'hours').subtract(1, 'seconds');
        let toDate = dayStart.format("YYYY-MM-DD HH:mm:ss");

        const logs = await EventLogs.query()
            .where('created_at', 'between', [`${fromDate}`, `${toDate}`])
            .orderBy('created_at', 'asc')
            .withGraphFetched('user');

        res.render('admin/index', {
            user: req.user,
            logs,
        });
    },
};