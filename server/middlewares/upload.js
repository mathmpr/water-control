const multer = require('multer');
const path = require('path');

const storage = multer.diskStorage({
    destination: (req, file, cb) => cb(null, 'public/uploads/'),
    filename: (req, file, cb) => {
        const ext = path.extname(file.originalname);
        const name = Date.now() + '-' + Math.round(Math.random() * 1e5) + ext;
        cb(null, name);
    }
});

const upload = multer({
    storage,
    limits: { fileSize: 5 * 1024 * 1024 }, // até 5MB
    fileFilter: (req, file, cb) => {
        const isImage = /jpeg|jpg|png|gif/.test(file.mimetype);
        if (isImage) cb(null, true);
        else cb(new Error('just images is allowed.'));
    }
});

module.exports = upload;
