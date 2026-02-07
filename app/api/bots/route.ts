import { addBot, listBots } from "../../../lib/botStore";
import type { Bot } from "../../../types/bot";

type BotCreateInput = Omit<Bot, "id" | "createdAt">;

function isValidInput(value: unknown): value is BotCreateInput {
  if (typeof value !== "object" || value === null) return false;
  const v = value as Record<string, unknown>;

  return (
    typeof v.name === "string" &&
    v.name.trim().length > 0 &&
    typeof v.exchange === "string" &&
    v.exchange.trim().length > 0 &&
    typeof v.strategy === "string" &&
    v.strategy.trim().length > 0 &&
    typeof v.pnl24h === "number" &&
    Number.isFinite(v.pnl24h) &&
    typeof v.winRate === "number" &&
    Number.isFinite(v.winRate) &&
    typeof v.trades === "number" &&
    Number.isFinite(v.trades)
  );
}

export function GET(): Response {
  return Response.json(listBots(), { status: 200 });
}

export async function POST(request: Request): Promise<Response> {
  let body: unknown;
  try {
    body = await request.json();
  } catch {
    return Response.json({ error: "Invalid JSON" }, { status: 400 });
  }

  if (!isValidInput(body)) {
    return Response.json(
      { error: "Missing or invalid fields" },
      { status: 400 },
    );
  }

  const bot: Bot = {
    id: crypto.randomUUID(),
    createdAt: new Date().toISOString(),
    ...body,
  };

  return Response.json(addBot(bot), { status: 201 });
}
