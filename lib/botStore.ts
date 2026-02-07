import type { Bot } from "../types/bot";

const bots: Bot[] = [];

export function addBot(bot: Bot): Bot {
  bots.push(bot);
  return bot;
}

export function listBots(): Bot[] {
  return [...bots];
}
