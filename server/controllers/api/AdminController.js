const ConfigService = require('../../services/ConfigService');

function ensureAdmin(req, res) {
    if (!req.user?.roles?.includes('admin')) {
        res.status(403).json({error: 'admin access required'});
        return false;
    }

    return true;
}

module.exports = {
    config: async (req, res) => {
        if (!ensureAdmin(req, res)) {
            return;
        }

        const config = await ConfigService.getConfig();

        res.json({config});
    },
    updateConfig: async (req, res) => {
        if (!ensureAdmin(req, res)) {
            return;
        }

        try {
            const config = await ConfigService.saveConfig(req.body);

            res.json({config});
        } catch (error) {
            res.status(422).json({error: error.message});
        }
    },
};
