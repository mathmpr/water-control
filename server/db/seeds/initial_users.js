let users;

try {
  users = require('./data/users');
} catch (error) {
  users = require('./data/users.exemple');
}

/**
 * @param { import("knex").Knex } knex
 * @returns { Promise<void> } 
 */
exports.seed = async function(knex) {
  for (const user of users) {
    const existing = await knex('users')
      .where({email: user.email})
      .orWhere({username: user.username})
      .first();

    if (!existing) {
      await knex('users').insert(user);
    }
  }
};
