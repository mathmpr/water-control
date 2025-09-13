const {Model} = require('objection');

class EventLogs extends Model {
    static get tableName() {
        return 'event_logs';
    }

    static get idColumn() {
        return 'id';
    }

    static get jsonSchema() {
        return {
            type: 'object',
            required: ['user_id'],

            properties: {
                id: {type: 'integer'},
                user_id: {type: 'integer'},
                value: {type: 'boolean'},
                trigger_type: {type: 'string'},
                income: {type: 'integer'},
                detect: {type: 'integer'},
                created_at: {type: 'string', format: 'date-time'},
                updated_at: {type: 'string', format: 'date-time'},
            },
        };
    }

    static get relationMappings() {
        const User = require('./User');
        return {
            user: {
                relation: Model.BelongsToOneRelation,
                modelClass: User,
                join: {
                    from: 'event_logs.user_id',
                    to: 'users.id',
                },
            },
        };
    }

    async getUser() {
        return this.$relatedQuery('user');
    }
}

module.exports = EventLogs;