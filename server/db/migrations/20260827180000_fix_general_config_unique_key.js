exports.up = async function(knex) {
    const hasTable = await knex.schema.hasTable('general_config');

    if (!hasTable) {
        return;
    }

    await knex.schema.alterTable('general_config', function(table) {
        table.dropUnique(['value']);
    });

    await knex.schema.alterTable('general_config', function(table) {
        table.unique(['config']);
    });
};

exports.down = async function(knex) {
    const hasTable = await knex.schema.hasTable('general_config');

    if (!hasTable) {
        return;
    }

    await knex.schema.alterTable('general_config', function(table) {
        table.dropUnique(['config']);
    });

    await knex.schema.alterTable('general_config', function(table) {
        table.unique(['value']);
    });
};
