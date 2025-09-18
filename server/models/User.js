const {Model} = require('objection');
const bcrypt = require('bcrypt');

class User extends Model {
    static get tableName() {
        return 'users';
    }

    static get idColumn() {
        return 'id';
    }

    static get jsonSchema() {
        return {
            type: 'object',
            required: ['name', 'email', 'username', 'password'],
            properties: {
                id: {type: 'integer'},
                name: {type: 'string'},
                email: {type: 'string', format: 'email'},
                username: {type: 'string'},
                password: {type: 'string'},
                created_at: {type: 'string'},
                updated_at: {type: 'string'},
                alexa_2fa: {type: 'string'},
                alexa_id: {type: 'string'},
            },
        };
    }

    async verifyPassword(requestPassword) {
        return bcrypt.compare(requestPassword, this.password);
    }
}

module.exports = User;