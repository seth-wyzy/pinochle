# Web prototype

Run `npm start` from the repository root, then open `http://localhost:8787`. The Node server serves this UI and keeps rooms in memory. Create a room, share its four-character code, and another browser can join an open AI seat. Clients poll the room state and send bid/play actions to the server.

The current server implements the network boundary and a playable bidding/trick loop with AI seats. State is intentionally in memory for development; use a database and WebSockets/SSE for production. The existing C++ game should next move its bidding, meld, trump, and trick logic into a UI-independent rules module; browser actions can then call that module through the server while AI turns continue to use `AIPlayer`.

Key endpoints are `POST /api/rooms`, `POST /api/rooms/:code/join`, `GET /api/rooms/:code?playerId=...`, and `POST /api/rooms/:code/actions`.

## Hosting

Deploy the repository to a container host such as Render, Railway, Fly.io, or a small VPS. The included `Dockerfile` runs the server and respects the host-provided `PORT`. For Render/Railway, create a web service from the repository and use `npm start` as the start command (or deploy the Dockerfile). Rooms are currently held in memory, so a restart clears active tables; add Redis or a database before treating the service as production-critical.
