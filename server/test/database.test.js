const { test, before, after } = require('node:test');
const assert = require('node:assert/strict');
const knex = require('knex');
const { Model } = require('objection');

if (process.env.TEST_MYSQL === '1') {
  if (!/^verify_[a-z0-9_]+$/.test(process.env.DB_DATABASE || '')) {
    throw new Error('MySQL tests require an explicit empty verify_* database');
  }
  process.env.DB_CLIENT = 'mysql2';
} else {
  process.env.DB_CLIENT = 'sqlite3';
  process.env.DATABASE_PATH = ':memory:';
}
const db = knex(require('../knexfile').test);
Model.knex(db);
before(async () => {
  if (process.env.TEST_MYSQL === '1') {
    const [tables] = await db.raw('SHOW TABLES');
    assert.equal(tables.length, 0, 'Refusing to test against a populated database');
  }
});
after(() => db.destroy());

const User = require('../models/User');
const EventLogs = require('../models/EventLogs');
const ConfigService = require('../services/ConfigService');
const bcrypt = require('bcrypt');

test('migrations, relations, passwords and pump configuration work', async () => {
  await db.migrate.latest();
  const hash = await bcrypt.hash('database-test-password', 4);
  const user = await User.query().insert({ name: 'Test', username: 'test', email: 'test@example.invalid', password: hash });
  assert(await user.verifyPassword('database-test-password'));
  const log = await EventLogs.query().insert({ user_id: user.id, value: true, trigger_type: 'manual', income: 10, detect: 20 });
  const stored = await EventLogs.query().findById(log.id).withGraphFetched('user');
  assert.equal(stored.user.id, user.id);
  assert.equal(Number(stored.value), 1);
  await assert.rejects(EventLogs.query().insert({ user_id: 999999, value: false }));
  const config = await ConfigService.saveConfig({ asker: 20, sender_income: 301, sender_top: 302, hours: '08:00,17:00' });
  assert.deepEqual(config.hours, ['08:00', '17:00']);
  await db.seed.run({ specific: 'general_config.js' });
  assert.equal((await ConfigService.getConfig()).asker, 20);
  assert.equal((await db.migrate.latest())[1].length, 0);
  await User.query().deleteById(user.id);
  assert.equal(await EventLogs.query().findById(log.id), undefined);
});
