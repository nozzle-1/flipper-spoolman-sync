#!/usr/bin/env node

const childProcess = require("child_process");
const fs = require("fs");
const path = require("path");

const BAMBUSTUDIO_REPO_URL = "https://github.com/bambulab/BambuStudio.git";
const DEFAULT_REPO_DIR = path.join(__dirname, "tmp", "BambuStudio");
const DEFAULT_SOURCE_DIR = path.join(
    DEFAULT_REPO_DIR,
    "resources",
    "profiles",
    "BBL",
    "filament",
);
const DEFAULT_OUTPUT_PATH = path.join(__dirname, "tmp", "bambu_lab_filaments.json");
const DEFAULT_VENDOR = "Bambu Lab";
const DEFAULT_WEIGHT = 1000;
const DEFAULT_SUPPORT_WEIGHT = 500;
const DEFAULT_SPOOL_WEIGHT = 250;
const DEFAULT_DIAMETER = 1.75;
const DEFAULT_LOCALE = "en";

// Allows manual color overrides when the Bambu source data does not match the spool's physical tag.
const FILAMENT_COLOR_EXCEPTIONS = {
    "PETG Translucent::Clear": ["00000000"], 
};

function parseArgs(argv) {
    const options = {
        repoDir: DEFAULT_REPO_DIR,
        sourceDir: DEFAULT_SOURCE_DIR,
        outputPath: DEFAULT_OUTPUT_PATH,
        locale: DEFAULT_LOCALE,
        vendor: DEFAULT_VENDOR,
        weight: DEFAULT_WEIGHT,
        spoolWeight: DEFAULT_SPOOL_WEIGHT,
        diameter: DEFAULT_DIAMETER,
    };

    for (let i = 0; i < argv.length; i += 1) {
        const arg = argv[i];
        const value = argv[i + 1];

        if (arg === "--source-dir" && value) {
            options.sourceDir = path.resolve(value);
            i += 1;
        } else if (arg === "--repo-dir" && value) {
            options.repoDir = path.resolve(value);
            options.sourceDir = path.join(
                options.repoDir,
                "resources",
                "profiles",
                "BBL",
                "filament",
            );
            i += 1;
        } else if (arg === "--vendor" && value) {
            options.vendor = value;
            i += 1;
        } else if (arg === "--weight" && value) {
            options.weight = Number(value);
            i += 1;
        } else if (arg === "--spool-weight" && value) {
            options.spoolWeight = Number(value);
            i += 1;
        } else if (arg === "--diameter" && value) {
            options.diameter = Number(value);
            i += 1;
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
  node tools/generate_bambu_lab_filaments_file.js [options]

Options:
  --repo-dir <path>      Path to the local BambuStudio clone
  --source-dir <path>    Path to the local Bambu Studio profiles directory
  --vendor <name>        Vendor name to inject (default: Bambu Lab)
  --weight <grams>       Default filament weight (default: 1000)
  --spool-weight <g>     Default empty spool weight (default: 250)
  --diameter <mm>        Default filament diameter (default: 1.75)
  --help                 Show this help message`);
}

function readFile(filePath) {
    return fs.readFileSync(filePath, "utf8");
}

function runGit(args, cwd) {
    childProcess.execFileSync("git", args, {
        cwd,
        stdio: "inherit",
    });
}

function toNumber(value, fallback = null) {
    if (Array.isArray(value)) {
        for (const item of value) {
            const parsed = toNumber(item, null);
            if (parsed !== null) {
                return parsed;
            }
        }
        return fallback;
    }

    if (value === undefined || value === null || value === "" || value === "nil") {
        return fallback;
    }

    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : fallback;
}

function listJsonFiles(sourceDir) {
    return fs
        .readdirSync(sourceDir)
        .filter((fileName) => fileName.endsWith(".json"))
        .sort((a, b) => a.localeCompare(b));
}

function pickBestProfile(profiles) {
    return profiles.reduce((bestProfile, profile) => {
        if (!bestProfile) {
            return profile;
        }

        const scoreDiff = scoreProfileName(profile.fileName) - scoreProfileName(bestProfile.fileName);
        if (scoreDiff !== 0) {
            return scoreDiff > 0 ? profile : bestProfile;
        }

        return profile.fileName.localeCompare(bestProfile.fileName) < 0 ? profile : bestProfile;
    }, null);
}

function scoreProfileName(fileName) {
    let score = 0;

    if (fileName.includes("@BBL X1C.json")) score += 100;
    if (fileName.includes("@BBL X1E.json")) score += 95;
    if (fileName.includes("@BBL P1S.json")) score += 90;
    if (fileName.includes("@BBL P1P.json")) score += 85;
    if (fileName.includes("@BBL A1.json")) score += 80;
    if (fileName.includes("@BBL A1M.json")) score += 75;
    if (!fileName.includes(" nozzle")) score += 50;
    if (fileName.includes("0.4 nozzle")) score += 25;
    if (fileName.includes("0.6 nozzle")) score += 20;
    if (fileName.includes("0.2 nozzle")) score += 15;
    if (fileName.includes("0.8 nozzle")) score += 10;

    return score;
}

function buildProfilesIndex(sourceDir, fileNames) {
    const baseFiles = new Map();
    const filamentIdToBaseNames = new Map();
    const inheritedProfiles = new Map();

    for (const fileName of fileNames) {
        const raw = readFile(path.join(sourceDir, fileName));
        const json = JSON.parse(raw);

        if (fileName.startsWith("Bambu") && fileName.endsWith("@base.json")) {
            const baseName = fileName.replace(/ @base\.json$/, "");
            baseFiles.set(baseName, { fileName, json });

            for (const filamentId of extractFilamentIds(raw)) {
                const baseNames = filamentIdToBaseNames.get(filamentId) || new Set();
                baseNames.add(baseName);
                filamentIdToBaseNames.set(filamentId, baseNames);
            }
            continue;
        }

        const inheritedBase = typeof json.inherits === "string" ? json.inherits.replace(/ @base$/, "") : null;
        if (!inheritedBase) {
            continue;
        }

        const profiles = inheritedProfiles.get(inheritedBase) || [];
        profiles.push({ fileName, json });
        inheritedProfiles.set(inheritedBase, profiles);
    }

    return { baseFiles, filamentIdToBaseNames, inheritedProfiles };
}

function extractFilamentIds(rawJson) {
    const filamentIds = new Set();
    const matches = rawJson.matchAll(/"filament_id"\s*:\s*"([^"]+)"/g);

    for (const match of matches) {
        const filamentId = match[1].replace(/\s+/g, "");
        if (filamentId) {
            filamentIds.add(filamentId);
        }
    }

    return filamentIds;
}

function resolveColorName(colorEntry, locale) {
    const names = colorEntry.fila_color_name || {};
    return (
        names[locale] ||
        names.en ||
        names.fr ||
        Object.values(names).find(Boolean) ||
        colorEntry.color_code ||
        "Unknown"
    );
}

function ensureSourceDir(sourceDir) {
    if (!fs.existsSync(sourceDir)) {
        throw new Error(`Source directory not found: ${sourceDir}`);
    }
}

function isSupportFilament(material) {
    return typeof material === "string" && material.startsWith("Support");
}

function resolveFilamentColors(colorEntry, locale) {
    const material = colorEntry.fila_type || "";
    const colorName = resolveColorName(colorEntry, locale);
    const exceptionKey = `${material}::${colorName}`;

    if (FILAMENT_COLOR_EXCEPTIONS[exceptionKey]) {
        return FILAMENT_COLOR_EXCEPTIONS[exceptionKey];
    }

    return Array.isArray(colorEntry.fila_color) ? colorEntry.fila_color : [];
}

function ensureLocalRepository(options) {
    const gitDir = path.join(options.repoDir, ".git");

    if (!fs.existsSync(gitDir)) {
        fs.mkdirSync(path.dirname(options.repoDir), { recursive: true });
        console.log(`Cloning BambuStudio into ${options.repoDir}...`);
        runGit(
            [
                "clone",
                "--depth",
                "1",
                "--filter=blob:none",
                "--sparse",
                BAMBUSTUDIO_REPO_URL,
                options.repoDir,
            ],
            __dirname,
        );
    }

    console.log("Preparing sparse-checkout...");
    runGit(
        [
            "-C",
            options.repoDir,
            "sparse-checkout",
            "set",
            "resources/profiles/BBL/filament",
        ],
        __dirname,
    );
}

function deleteLocalRepository(options) {
    if (fs.existsSync(options.repoDir)) {
        fs.rmSync(options.repoDir, { recursive: true, force: true });
        console.log(`Deleted local repository: ${options.repoDir}`);
    }
}

function generate(options) {
    ensureLocalRepository(options);

    try {
        ensureSourceDir(options.sourceDir);

        const colorCodesPath = path.join(options.sourceDir, "filaments_color_codes.json");
        const colorCodesJson = JSON.parse(readFile(colorCodesPath));
        const fileNames = listJsonFiles(options.sourceDir);
        const { baseFiles, filamentIdToBaseNames, inheritedProfiles } = buildProfilesIndex(options.sourceDir, fileNames);

        const warnings = [];
        const filaments = [];

        for (const colorEntry of colorCodesJson.data || []) {
            const matchingBaseNames = filamentIdToBaseNames.get(colorEntry.fila_id);
            if (!matchingBaseNames || matchingBaseNames.size === 0) {
                continue;
            }

            const [baseName] = [...matchingBaseNames].sort((left, right) => left.localeCompare(right));
            const baseProfile = baseFiles.get(baseName);

            if (!baseProfile) {
                warnings.push(`Base profile not found for ${colorEntry.fila_id} (${baseName})`);
                continue;
            }

            const profileJson = pickBestProfile(inheritedProfiles.get(baseName) || [])?.json || null;
            const baseExtruderTemp =
                toNumber(baseProfile.json.nozzle_temperature, null) ??
                toNumber(baseProfile.json.nozzle_temperature_initial_layer, null);

            if (!profileJson && baseExtruderTemp === null) {
                warnings.push(`Printer profile not found for ${baseName}`);
            }

            const material = colorEntry.fila_type;

            filaments.push({
                filament_id: colorEntry.fila_id,
                material,
                color_name: resolveColorName(colorEntry, options.locale),
                colors: resolveFilamentColors(colorEntry, options.locale),
                vendor: Array.isArray(baseProfile.json.filament_vendor) && baseProfile.json.filament_vendor[0]
                    ? baseProfile.json.filament_vendor[0]
                    : options.vendor,
                weight: isSupportFilament(material) ? DEFAULT_SUPPORT_WEIGHT : options.weight,
                spool_weight: options.spoolWeight,
                filament_cost: toNumber(baseProfile.json.filament_cost, 0),
                density: toNumber(baseProfile.json.filament_density, null),
                diameter: options.diameter,
                extruder_temp:
                    toNumber(profileJson?.nozzle_temperature, null) ??
                    toNumber(profileJson?.nozzle_temperature_initial_layer, null) ??
                    baseExtruderTemp,
            });
        }

        const output = { filaments, total: filaments.length };
        fs.mkdirSync(path.dirname(options.outputPath), { recursive: true });
        fs.writeFileSync(options.outputPath, `${JSON.stringify(output, null, 4)}\n`, "utf8");

        return {
            outputPath: options.outputPath,
            count: filaments.length,
            warnings,
        };
    } finally {
        deleteLocalRepository(options);
    }
}

function main() {
    try {
        const options = parseArgs(process.argv.slice(2));
        const result = generate(options);
        console.log(`Generated file: ${result.outputPath}`);
        console.log(`Entries: ${result.count}`);

        if (result.warnings.length > 0) {
            console.warn(`Warnings: ${result.warnings.length}`);
            for (const warning of result.warnings) {
                console.warn(`- ${warning}`);
            }
        }
    } catch (error) {
        console.error(error.message);
        process.exit(1);
    }
}

main();
