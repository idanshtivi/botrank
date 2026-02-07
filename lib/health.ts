import type { HealthResponse } from "../types/health";

export function getHealth(): HealthResponse {
  return {
    ok: true,
    name: "BotRank",
    ts: new Date().toISOString(),
  };
}
