# Web prototype

Set `DATABASE_URL` to a PostgreSQL connection string, then run `npm install` and `npm start` from the repository root. The server applies the SQL migration at startup and serves the UI at `http://localhost:8787`. Create a room, share its eight-character code, and another browser can join an open AI seat. Clients poll the room state and send bid/play actions to the server.

Rooms, seats, and the activity history are persisted in PostgreSQL for 30 days. A browser-held, 256-bit seat token is stored in local storage and is required to read or act for a seat; it is returned only when that seat is created or joined. Timed AI and phase transitions are persisted as due actions, so a later poll safely resumes them after a restart or cold start.

Key endpoints are `POST /api/rooms`, `POST /api/rooms/:code/join`, `GET /api/rooms/:code?seatToken=...`, and `POST /api/rooms/:code/actions`.

## Hosting

For a no-cost hosted setup, create a free Neon Postgres project and provide its pooled connection string as `DATABASE_URL` when Render creates the Blueprint. The Blueprint only provisions Render's free web service; it does not create a Render database. The service runs migrations before listening and exposes `GET /health` for Render health checks. Free services can cold-start after inactivity, but active games resume from their stored snapshot.
