const path = require('path');

module.exports = {
    development: {
        client: 'sqlite3',
        connection: {
            filename: 'dev.sqlite3'
        },
        useNullAsDefault: true,
        migrations: {
            directory: 'db/migrations'
        },
        seeds: {
            directory: 'db/seeds'
        },
        pool: {
            afterCreate: (conn, done) => {
                conn.run('PRAGMA foreign_keys = ON', done);
            }
        }
    }
};