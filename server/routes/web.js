const express = require('express');
const router = express.Router();

const authMiddleware = require('../middlewares/auth');
const signedMiddleware = require('../middlewares/signed');

const SignController = require('../controllers/web/SignController');
const AdminController = require('../controllers/web/AdminController');

router.get('/', signedMiddleware, SignController.signIn);
router.get('/sign-in', signedMiddleware, SignController.signIn);
router.get('/sign-up', signedMiddleware, SignController.signUp);

router.get('/admin', authMiddleware, AdminController.index);

module.exports = router;