/* BACKEND: server.js */
const express = require("express");
const fs = require("fs");
const cors = require("cors");

const app = express();
const PORT = 3000;

app.use(express.json());
app.use(cors());

let scores = { 
    fallingBlocks: [],
    bubbleShooter: [],
    slidingPuzzle: []
};

// Save score
app.post("/save-score", (req, res) => {
    const { game, player, score } = req.body;
    
    if (!scores[game]) {
        return res.status(400).json({ message: "Invalid game name" });
    }

    scores[game].push({ player, score });

    fs.writeFileSync("scores.json", JSON.stringify(scores, null, 2));
    res.json({ message: "Score saved successfully!" });
});

// Get scores
app.get("/scores", (req, res) => {
    res.json(scores);
});

app.listen(PORT, () => console.log(`Server running on port ${PORT}`));