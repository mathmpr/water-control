const {defaults} = require('../../services/ConfigService');

exports.seed = async function(knex) {
    for (const [config, value] of Object.entries(defaults)) {
        const existing = await knex('general_config').where({config}).first();

        if (!existing) {
            await knex('general_config').insert({config, value});
        }
    }
};
