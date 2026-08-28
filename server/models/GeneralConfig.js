const {Model} = require('objection');

class GeneralConfig extends Model {
    static get tableName() {
        return 'general_config';
    }

    static get idColumn() {
        return 'id';
    }

    static get jsonSchema() {
        return {
            type: 'object',
            required: ['config', 'value'],
            properties: {
                id: {type: 'integer'},
                config: {type: 'string'},
                value: {type: 'string'},
                created_at: {type: 'string'},
                updated_at: {type: 'string'},
            },
        };
    }
}

module.exports = GeneralConfig;
