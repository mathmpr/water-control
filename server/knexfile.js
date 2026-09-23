const path = require('path');
require('dotenv').config({ quiet: true });

// DATABASE_PATH keeps existing deployments readable during the migration.
// New installations default to MySQL; DB_CLIENT always takes precedence.
const client = process.env.DB_CLIENT || (process.env.DATABASE_PATH ? 'sqlite3' : 'mysql2');
const common = {
  migrations: { directory: path.join(__dirname, 'db/migrations') },
  seeds: { directory: path.join(__dirname, 'db/seeds') }
};

let config;
if (client === 'mysql2') {
  for (const name of ['DB_HOST', 'DB_DATABASE', 'DB_USERNAME', 'DB_PASSWORD']) {
    if (!process.env[name]) throw new Error(name + ' is required for MySQL');
  }
  const port = Number(process.env.DB_PORT || 3306);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error('DB_PORT must be a valid TCP port');
  }
  config = {
    ...common,
    client: 'mysql2',
    connection: {
      host: process.env.DB_HOST,
      port,
      database: process.env.DB_DATABASE,
      user: process.env.DB_USERNAME,
      password: process.env.DB_PASSWORD,
      charset: 'utf8mb4',
      timezone: 'Z'
    },
    pool: {
      min: 0,
      max: 5,
      afterCreate(connection, done) {
        connection.query("SET time_zone = '+00:00'", error => done(error, connection));
      }
    },
    acquireConnectionTimeout: 15000
  };
} else if (client === 'sqlite3') {
  config = {
    ...common,
    client: 'sqlite3',
    connection: { filename: process.env.DATABASE_PATH || path.join(__dirname, 'dev.sqlite3') },
    useNullAsDefault: true,
    pool: {
      afterCreate(connection, done) {
        connection.run('PRAGMA foreign_keys = ON', error => done(error, connection));
      }
    }
  };
} else {
  throw new Error('DB_CLIENT must be mysql2 or sqlite3');
}

module.exports = { development: config, production: config, test: config };
