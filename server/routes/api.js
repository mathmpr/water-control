const express = require('express');
const router = express.Router();

const SignController = require('../controllers/api/SignController');


router.post('/sign-in', SignController.signIn);
router.get('/sign-out', SignController.signOut);

module.exports = router;