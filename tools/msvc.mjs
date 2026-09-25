// Normalize environment key casing: some Windows launchers supply Path and PATH.
import { spawnSync } from 'node:child_process';
const env = {};
for (const [key,value] of Object.entries(process.env)) env[key.toUpperCase()] = value;
const args = process.argv.slice(2);
const result = spawnSync(args.shift() || 'cmake', args, { env, stdio: 'inherit' });
if (result.error) console.error(result.error);
process.exit(result.status ?? 1);
