import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
var VISUALIZER_PORT = 5173;
export default defineConfig({
    plugins: [react()],
    server: {
        host: "0.0.0.0",
        port: VISUALIZER_PORT,
        strictPort: true,
    },
    preview: {
        host: "0.0.0.0",
        port: VISUALIZER_PORT,
        strictPort: true,
    },
});
