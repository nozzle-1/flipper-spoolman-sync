#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

const DEFAULT_VENDOR_NAME = "Bambu Lab";
const DEFAULT_EMPTY_SPOOL_WEIGHT = 250;
const DEFAULT_INPUT_PATH = path.join(__dirname, "tmp", "bambu_lab_filaments.json");
const ANSI_ORANGE = "\x1b[38;5;214m";
const ANSI_GREEN = "\x1b[32m";
const ANSI_RESET = "\x1b[0m";
const DENSITY_FALLBACK_RULES = [
    { prefix: "Support for PA", density: 1.17 },
    { prefix: "Support for ABS", density: 1.16 },
    { prefix: "Support for PLA", density: 1.33 },
    { prefix: "PPA", density: 1.25 },
    { prefix: "PETG", density: 1.25 },
    { prefix: "PET", density: 1.29 },
    { prefix: "PLA", density: 1.24 },
    { prefix: "TPU", density: 1.22 },
    { prefix: "ABS", density: 1.05 },
    { prefix: "ASA", density: 1.05 },
    { prefix: "PA", density: 1.14 },
    { prefix: "PC", density: 1.20 },
    { prefix: "PVA", density: 1.27 },
    { prefix: "PPS", density: 1.26 },
];

function parseArgs(argv) {
    const options = {
        baseUrl: "",
        vendorName: DEFAULT_VENDOR_NAME,
        inputPath: DEFAULT_INPUT_PATH,
        dryRun: false,
        trace: false,
    };

    for (let index = 0; index < argv.length; index += 1) {
        const arg = argv[index];
        const value = argv[index + 1];

        if (arg === "--url" && value) {
            options.baseUrl = value;
            index += 1;
        } else if (arg === "--file" && value) {
            options.inputPath = path.resolve(value);
            index += 1;
        } else if (arg === "--vendor" && value) {
            options.vendorName = value;
            index += 1;
        } else if (arg === "--dry-run") {
            options.dryRun = true;
        } else if (arg === "--trace") {
            options.trace = true;
        } else if (arg === "--help") {
            printHelp();
            process.exit(0);
        } else {
            throw new Error(`Unknown argument: ${arg}`);
        }
    }

    return options;
}

function printHelp() {
    console.log(`Usage:
  node tools/import_bambu_lab_filaments_to_spoolman.js [options]

Options:
  --url <url>       Required Spoolman base URL
  --file <path>     Source JSON file (default: ${DEFAULT_INPUT_PATH})
  --vendor <name>   Vendor name to create or reuse (default: ${DEFAULT_VENDOR_NAME})
  --dry-run         Simulate the import without writing to Spoolman
  --trace           Print JSON payloads sent to the API
  --help            Show this help message`);
}

function normalizeBaseUrl(baseUrl) {
    const trimmed = String(baseUrl || "").trim().replace(/\/+$/, "");
    if (!trimmed) {
        throw new Error("Spoolman URL is empty.");
    }

    return trimmed.endsWith("/api/v1") ? trimmed : `${trimmed}/api/v1`;
}

function readInputFile(filePath) {
    const payload = JSON.parse(fs.readFileSync(filePath, "utf8"));

    if (!payload || !Array.isArray(payload.filaments)) {
        throw new Error("The source file must contain a filaments array.");
    }

    return payload.filaments;
}

async function apiRequest(baseApiUrl, pathname, options = {}) {
    const url = `${baseApiUrl}${pathname}`;
    if (options.trace && options.body !== undefined) {
        console.log(
            `${ANSI_GREEN}TRACE  ${options.method || "GET"} ${pathname} ${JSON.stringify(options.body)}${ANSI_RESET}`,
        );
    }

    const response = await fetch(url, {
        method: options.method || "GET",
        headers: {
            Accept: "application/json",
            "Content-Type": "application/json",
            ...(options.headers || {}),
        },
        body: options.body !== undefined ? JSON.stringify(options.body) : undefined,
    });
    const text = await response.text();
    let data = null;

    if (text) {
        try {
            data = JSON.parse(text);
        } catch (error) {
            data = text;
        }
    }

    if (!response.ok) {
        const details = typeof data === "string" ? data : JSON.stringify(data);
        throw new Error(
            `HTTP ${response.status} ${response.statusText} on ${options.method || "GET"} ${pathname}: ${details}`,
        );
    }

    return data;
}

function traceRequest(pathname, method, body, trace) {
    if (!trace || body === undefined) {
        return;
    }

    console.log(
        `${ANSI_GREEN}TRACE  ${method} ${pathname} ${JSON.stringify(body)}${ANSI_RESET}`,
    );
}

function normalizeText(value) {
    return String(value || "").trim().toLowerCase();
}

function normalizeSingleColorHex(color) {
    const normalized = String(color || "").trim().replace(/^#/, "").slice(0, 8).toUpperCase();
    return normalized || null;
}

function normalizeColorHexes(colors) {
    if (!Array.isArray(colors) || colors.length === 0) {
        return [];
    }

    const normalizedColors = [];

    for (const color of colors) {
        const normalizedColor = normalizeSingleColorHex(color);
        if (!normalizedColor || normalizedColors.includes(normalizedColor)) {
            continue;
        }

        normalizedColors.push(normalizedColor);
    }

    return normalizedColors;
}

function toPositiveNumber(value) {
    const numericValue = Number(value);
    return numericValue > 0 ? numericValue : undefined;
}

function getDensityFallback(material) {
    const normalizedMaterial = String(material || "").trim();
    for (const rule of DENSITY_FALLBACK_RULES) {
        if (normalizedMaterial.startsWith(rule.prefix)) {
            return rule;
        }
    }

    return null;
}

function buildFilamentName(entry) {
    return String(entry.color_name || "").trim();
}

async function findVendor(baseApiUrl, vendorName) {
    const encodedName = encodeURIComponent(vendorName);
    const vendors = await apiRequest(baseApiUrl, `/vendor?name=${encodedName}&limit=1`);

    return vendors.find((vendor) => normalizeText(vendor.name) === normalizeText(vendorName)) || null;
}

async function ensureVendor(baseApiUrl, vendorName, dryRun, trace) {
    const existingVendor = await findVendor(baseApiUrl, vendorName);
    if (existingVendor) {
        return { vendor: existingVendor, created: false };
    }

    const body = { name: vendorName, empty_spool_weight: DEFAULT_EMPTY_SPOOL_WEIGHT };
    if (dryRun) {
        traceRequest("/vendor", "POST", body, trace);
        return {
            vendor: { id: -1, name: vendorName },
            created: true,
        };
    }

    const vendor = await apiRequest(baseApiUrl, "/vendor", {
        method: "POST",
        body,
        trace,
    });

    return { vendor, created: true };
}

async function fetchFilamentPage(baseApiUrl, params) {
    const searchParams = new URLSearchParams();
    for (const [key, value] of Object.entries(params)) {
        if (value === undefined || value === null || value === "") {
            continue;
        }
        searchParams.set(key, String(value));
    }

    const page = await apiRequest(baseApiUrl, `/filament?${searchParams.toString()}`);
    if (!Array.isArray(page)) {
        throw new Error("Unexpected response from the /filament API.");
    }

    return page;
}

async function findExistingFilament(baseApiUrl, vendorName, payload) {
    const params = {
        name: payload.name,
        "vendor.name": vendorName,
        article_number: payload.article_number,
        color_hex: payload.color_hex ? payload.color_hex : payload.multi_color_hexes,
        color_similarity_threshold: 0,
        limit: 1,
        offset: 0,
    };

    const page = await fetchFilamentPage(baseApiUrl, params);
    return page[0] || null;
}

function buildFilamentPayload(entry, vendorId) {
    const densityFallbackRule = getDensityFallback(entry.material);
    const density = toPositiveNumber(entry.density) ?? densityFallbackRule?.density;
    const colorHexes = normalizeColorHexes(entry.colors);
    const payload = {
        vendor_id: vendorId,
        name: buildFilamentName(entry),
        material: entry.material,
        density,
        diameter: toPositiveNumber(entry.diameter),
    };

    const optionalFields = {
        price: entry.filament_cost,
        weight: entry.weight,
        spool_weight: entry.spool_weight,
        article_number: entry.filament_id,
        settings_extruder_temp: entry.extruder_temp,
        settings_bed_temp: entry.bed_temp,
        color_hex: colorHexes.length > 1 ? null : colorHexes[0] || null,
        multi_color_direction: colorHexes.length > 1 ? "coaxial" : null,
        multi_color_hexes: colorHexes.length > 1 ? colorHexes.join(",") : undefined,
    };

    for (const [key, value] of Object.entries(optionalFields)) {
        if (value === null || value === undefined || value === "") {
            continue;
        }

        payload[key] = value;
    }

    return payload;
}

function validateFilamentPayload(payload, sourceEntry) {
    if (!payload.name) {
        throw new Error(`Missing color_name for ${JSON.stringify(sourceEntry)}`);
    }
    if (!payload.material) {
        throw new Error(`Missing material for ${JSON.stringify(sourceEntry)}`);
    }
    if (payload.density === undefined) {
        throw new Error(`Missing density for ${payload.material} / ${payload.name}`);
    }
    if (payload.diameter === undefined) {
        throw new Error(`Missing diameter for ${payload.material} / ${payload.name}`);
    }
}

async function createFilament(baseApiUrl, payload, dryRun, trace) {
    if (dryRun) {
        traceRequest("/filament", "POST", payload, trace);
        return { id: -1, ...payload };
    }

    return apiRequest(baseApiUrl, "/filament", {
        method: "POST",
        body: payload,
        trace,
    });
}

async function main() {
    const options = parseArgs(process.argv.slice(2));
    if (!options.baseUrl) {
        throw new Error("The --url parameter is required. Use --help to see usage.");
    }

    const baseApiUrl = normalizeBaseUrl(options.baseUrl);
    const inputFilaments = readInputFile(options.inputPath);

    console.log(`Source: ${options.inputPath}`);
    console.log(`API: ${baseApiUrl}`);
    console.log(`Vendor: ${options.vendorName}`);
    console.log(`Mode: ${options.dryRun ? "dry-run" : "write"}`);
    console.log(`Trace: ${options.trace ? "on" : "off"}`);

    const { vendor, created: vendorCreated } = await ensureVendor(
        baseApiUrl,
        options.vendorName,
        options.dryRun,
        options.trace,
    );

    console.log(
        vendorCreated
            ? `Created vendor: ${vendor.name}${vendor.id ? ` (id=${vendor.id})` : ""}`
            : `Using existing vendor: ${vendor.name} (id=${vendor.id})`,
    );

    const stats = {
        scanned: 0,
        created: 0,
        densityFallbacks: 0,
        skipped: 0,
    };

    for (const entry of inputFilaments) {
        const payload = buildFilamentPayload(entry, vendor.id);
        validateFilamentPayload(payload, entry);

        stats.scanned += 1;

        const existingFilament =
            vendor.id === -1 ? null : await findExistingFilament(baseApiUrl, options.vendorName, payload);

        if (!existingFilament) {
            const densityFallbackRule = getDensityFallback(entry.material);
            if (!toPositiveNumber(entry.density) && densityFallbackRule) {
                stats.densityFallbacks += 1;
                console.log(
                    `${ANSI_ORANGE}WARN   ${payload.material} / ${payload.name} -> density fallback ${densityFallbackRule.prefix}=${payload.density}${ANSI_RESET}`,
                );
            }
            await createFilament(baseApiUrl, payload, options.dryRun, options.trace);
            stats.created += 1;
            console.log(`CREATE ${payload.material} / ${payload.name} -> article_number=${payload.article_number}`);
            continue;
        }

        stats.skipped += 1;
        console.log(
            `SKIP   ${payload.material} / ${payload.name} -> already present in Spoolman`,
        );
    }

    console.log("");
    console.log("Summary:");
    console.log(`  Filaments read: ${stats.scanned}`);
    console.log(`  Created: ${stats.created}`);
    console.log(`  Density fallbacks used: ${stats.densityFallbacks}`);
    console.log(`  Already present: ${stats.skipped}`);
}

main().catch((error) => {
    console.error(error.message);
    process.exit(1);
});
