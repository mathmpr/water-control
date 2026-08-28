const express = require('express');
const router = express.Router();

const SignController = require('../controllers/api/SignController');
const AlexaController = require('../controllers/api/AlexaController');
const AdminController = require('../controllers/api/AdminController');
const authMiddleware = require('../middlewares/auth');

router.get('/health', (_req, res) => res.json({ok: true}));

router.post('/sign-in', SignController.signIn);
router.get('/sign-out', SignController.signOut);

router.all('/alexa', AlexaController.index);
router.get('/admin/config', authMiddleware, AdminController.config);
router.post('/admin/config', authMiddleware, AdminController.updateConfig);

module.exports = router;
