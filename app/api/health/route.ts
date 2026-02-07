import { getHealth } from "../../../lib/health";
import type { HealthResponse } from "../../../types/health";

export function GET(): Response {
  const body: HealthResponse = getHealth();

  return Response.json(body, { status: 200 });
}
