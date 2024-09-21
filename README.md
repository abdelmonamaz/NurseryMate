# 🌱 Nursera — Pépinière Idéale

Application de gestion pour **Pépinière Idéale** (*Vente, Aménagement & Entretien*, Tunisie).
Desktop Windows + Android · C++23 / QML / Qt 6.11 · Documentation complète : [docs/](docs/README.md)

## Structure

```
apps/desktop/     Gestion & caisse (Windows) — connexion PIN, tableau de bord,
                  pages Caisse, Catalogue, Stock, Inventaire, Clients, Achats,
                  Réglages ; composants app (AppHeader, AppSidebar…) séparés
apps/mobile/      Compagnon terrain (Android, compile aussi en desktop pour preview)
libs/core/        Result, Money (millimes), PasswordHasher (PBKDF2),
                  DatabaseManager + migrations SQL, BackupService (VACUUM+rotation),
                  models, repositories I* + sqlite/ (recordMove transactionnel)
libs/services/    Controllers (Auth, Catalog, Stock, Inventory, Sale, Dashboard,
                  Customer, Supplier, Admin, Invoice, Batch, Quote) + modèles QML
                  + generators/ (ticket, facture, devis PDF, étiquettes QR)
                  + thirdparty/qrcodegen (MIT)
libs/ui-common/   Design system QML (Nursera.UI) : NTheme, NButton, NCard, NBadge,
                  NTextField, NComboBox, NDialog, NListRow, NEmptyState, NFieldLabel,
                  NLineChart (courbe), NBarChart (diagramme)
tests/unit/       Qt Test : money, database, catalog, stock, inventory, auth,
                  sales, ticket, dashboard, customers, suppliers, admin,
                  amount_words, invoice, pos, batches, labels, quotes
```

Schéma de base : [docs/04-schema-bdd.md](docs/04-schema-bdd.md) (document vivant).

## Build (ligne de commande) — MSVC 2022 (officiel desktop)

```powershell
$env:PATH = "C:\Qt\Tools\CMake_64\bin;$env:PATH"

cmake --preset desktop-msvc            # générateur Visual Studio 17 2022, pas de vcvars requis
cmake --build --preset desktop-debug   # ou desktop-release

# Tests (multi-config : préciser -C)
$env:PATH = "C:\Qt\6.11.0\msvc2022_64\bin;$env:PATH"
ctest --test-dir build/desktop-msvc -C Debug --output-on-failure

# Lancer
build\desktop-msvc\apps\desktop\Debug\nursera-desktop.exe
```

Presets MinGW conservés en secours : `desktop-mingw-debug` / `desktop-mingw-release`.

## Installateur Windows

```powershell
.\build_installer.ps1   # Release + windeployqt + Inno Setup 6
```

Produit `Output\Nursera_Installer_<version>.exe` (version = dernier tag git `v*`,
repli 0.1.0). Aucune donnée de démo embarquée : la base est créée au premier
lancement dans `%APPDATA%\Pepiniere Ideale\Nursera` (écran de création du Gérant) ;
la désinstallation ne touche jamais aux données.

## Build (Qt Creator)

Ouvrir `CMakeLists.txt` avec les kits :
- **Desktop Qt 6.11.0 MSVC2022 64bit** (officiel) → cibles `nursera-desktop`, `nursera-mobile`
- **Qt 6.11.0 pour Android arm64-v8a** → cible `nursera-mobile` (APK)

## Sync mobile (M09)

Le poste principal (case « 📡 Poste principal » dans Réglages) embarque un
serveur LAN (port 8477 par défaut, HTTP/1.1 minimal sur QTcpServer —
`libs/sync/`). Protocole v1 (voir `sync_server.h`) : `GET /api/v1/ping`,
`POST /api/v1/auth` (user+PIN → jeton Bearer), `GET /api/v1/catalog`,
`GET /api/v1/stock`, `POST /api/v1/moves` (idempotent par uuid).

Le compagnon terrain (`apps/mobile`, aussi exécutable en desktop pour la
preview) est **offline-first** : réplique locale du référentiel + stock
(`MobileStore`), saisies (transfert, perte) mises en file et poussées à la
sync (`SyncClient`) — jamais besoin du réseau pour saisir. Connexion par
IP + utilisateur + PIN (l'IP est affichée dans Réglages du poste principal).
Reste : inventaire mobile, appairage QR + mDNS (F09-09), photos.

## Conventions

- Montants : type `Money` en millimes (`qint64`) — jamais de `double` (RT-01).
- Migrations : `libs/core/migrations/NNN_description.sql`, appliquées au démarrage, suivies par `PRAGMA user_version`.
- Traductions : source FR, cible AR (`i18n/nursera_ar.ts`), cibles `update_translations` / `release_translations`.
- Versionnage : tag git `vX.Y.Z` (repris automatiquement par CMake).
- Architecture : voir [docs/02-specifications-techniques.md](docs/02-specifications-techniques.md) §2.
- Dev : `NURSERA_DEV_USER` + `NURSERA_DEV_PIN` (variables d'environnement) connectent
  automatiquement au lancement — passe par la vraie vérification du PIN.
  `NURSERA_GEN_INVOICE=<saleId>` génère et ouvre la facture d'une vente (support).
