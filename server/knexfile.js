const path = require('path');

const sqliteConfig = {
    client: 'sqlite3',
    connection: {
        filename: process.env.DATABASE_PATH || path.join(__dirname, 'dev.sqlite3')
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
};

module.exports = {
    development: sqliteConfig,
    production: sqliteConfig
};
