exports.up = function(knex) {
    return knex.schema.alterTable('users', table => {
        table.string('alexa_2fa').nullable();
        table.text('alexa_id').nullable();
    });
};

exports.down = function(knex) {
    return knex.schema.alterTable('users', table => {
        table.dropColumn('alexa_2fa');
        table.dropColumn('alexa_id');
    });
};
