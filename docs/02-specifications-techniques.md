# 🛠 Nursera — Spécifications techniques

> Version 1.0 · 14/07/2026 · Réf. : [00-vision-et-grands-axes.md](00-vision-et-grands-axes.md) · [01-specifications-fonctionnelles.md](01-specifications-fonctionnelles.md)

---

## 1. Stack technique

> **Aligné sur l'environnement validé** par les projets de référence `GestionScolaire_QML` (desktop, MinGW) et `ftour_jomaa` (Android) qui compilent tous deux avec ce toolchain.

| Composant | Choix | Justification |
|---|---|---|
| Langage | **C++23** | Déjà utilisé dans GestionScolaire (`CMAKE_CXX_STANDARD 23`), validé avec MinGW 64-bit |
| Framework | **Qt 6.11.0** | Version installée et éprouvée sur les deux projets de référence (kits Desktop MinGW 64-bit + Android arm64-v8a / x86_64) |
| UI | **QML / Qt Quick Controls 2** | UI commune desktop + Android, theming custom, RTL natif (`LayoutMirroring`) ; modules QML séparés `components`/`pages` comme dans GestionScolaire |
| Base de données | **SQLite 3** (via `QSqlDatabase`, driver `QSQLITE`) | Zéro administration, fiable, mode WAL, adapté PME ; pattern `database_manager` + `database_worker` (thread dédié) déjà éprouvé dans GestionScolaire |
| Serveur LAN | **QHttpServer** (module Qt) + REST/JSON | Embarqué dans l'app desktop principale, pas de service tiers |
| Build | **CMake ≥ 3.16** + Ninja | Même socle que GestionScolaire (versionnage par tag git `v*`, politiques QTP0001/QTP0004) |
| Tests | **Qt Test** (C++ + QML `TestCase`) | Intégré, exécutable en CI |
| Scan codes | **ZXing-C++** (intégré via FetchContent) | QR + EAN, licence Apache 2.0 |
| PDF / impression | `QPrinter` + `QPdfWriter` + QML rendu vers PDF | Factures A4, tickets 80 mm |
| Hachage PIN/MDP | **Argon2id** (libsodium) ou PBKDF2 (`QPasswordDigestor`) en repli | Sécurité des secrets locaux |
| Licences | Qt **LGPLv3, liaison dynamique** | Coût nul ; obligation : liens dynamiques + mention des licences |

**Cibles** : Windows 10/11 x64 — kit **Desktop Qt 6.11 MinGW 64-bit** (desktop principal), Android 9+ (API 28+) — kits **Qt 6.11 pour Android `arm64-v8a`** (production) et **`x86_64`** (émulateur), identiques à ceux de `ftour_jomaa`.

---

## 2. Architecture applicative

### 2.1 Vue d'ensemble

Un **seul projet**, trois exécutables issus d'un socle commun :

```
┌────────────────────────────────────────────────────────────┐
│                        apps/                               │
│      nursera-desktop                nursera-mobile         │
│      (QML desktop)                  (QML Android)          │
├────────────────────────────────────────────────────────────┤
│                        libs/                               │
│  ui-common/     Module QML : thème, composants N*, i18n    │
│  services/      Services métier + controllers (QObject)    │
│  core/          common (Result, Money), models,            │
│                 repositories (I* + sqlite/), database      │
│  sync/          Serveur LAN (QHttpServer) + client de sync │
└────────────────────────────────────────────────────────────┘
```

> **Note** : pas de serveur séparé en V1 — l'app desktop du poste principal embarque le serveur LAN (case « Ce poste est le poste principal » dans les paramètres). Une cible headless pourra être extraite en V2 (la lib `sync/` est indépendante de l'UI).

### 2.2 Découpage en couches (dans chaque écran)

Même pattern que GestionScolaire (`controllers → services → repositories (interfaces + impl. sqlite) → database_manager/worker`) :

```
QML (View)  ──binding──▶  Controller (QObject, Q_PROPERTY, Q_INVOKABLE)
                              │ appelle
                          Service métier (domain/) — règles de gestion, transactions
                              │ utilise
                          Repository (data/) — interface I*Repository + impl. sqlite/
                              │
                          DatabaseManager / DatabaseWorker (thread E/S) → SQLite (WAL)
```

**Règles d'architecture** :
- **Aucune logique métier en QML** : le QML lie des propriétés et appelle des `Q_INVOKABLE` ; tout calcul (totaux, TVA, arrondis) vit en C++.
- Les listes passent par des modèles C++ (`QAbstractListModel`) — jamais de `ListModel` QML pour des données métier.
- Un controller par écran (ex. `SaleController`, `InventoryController`) + un `AppController` racine, comme dans GestionScolaire.
- Repositories derrière des **interfaces** (`IProductRepository`, …) avec implémentation `sqlite/` — pattern éprouvé, testable, déjà pratiqué.
- Les services renvoient `Result<T>` (valeur ou erreur typée + message traduisible, cf. `src/common/result.h` de GestionScolaire) — pas d'exceptions à travers la frontière QML.
- Injection de dépendances manuelle par constructeur (pas de framework DI) : `main.cpp` compose l'application.

### 2.3 Type monétaire

Interdiction du `double` pour les montants. Type valeur `Money` :

```cpp
class Money {          // montant en millimes (int64)
    qint64 millimes_;
public:
    static Money fromTnd(qint64 dinars, int millimes = 0);
    Money operator+(Money) const; /* … */
    Money applyPercent(int bp) const;   // basis points, arrondi demi-sup
    QString toDisplayString(const QLocale&) const;  // "12,500 DT" / "د.ت"
};
```

TVA et remises calculées en millimes avec arrondi demi-supérieur (RT-01).

---

## 3. Modèle de données (SQLite)

### 3.1 Tables principales

```
users(id, username, display_name, role, pin_hash, pwd_hash, lang, active, created_at)
devices(id, name, kind, token_hash, last_sync_at, revoked)

categories(id, parent_id, name_fr, name_ar, sort_order, active)
products(id, category_id, name_fr, name_ar, botanical_name, type,           -- plant|goods
         description_fr, description_ar, sun, water_need, hardiness,
         flowering_period, planting_period, adult_height, active)
product_photos(id, product_id, file_path, is_main, taken_at, taken_by)
variants(id, product_id, sku, barcode, packaging,                            -- godet|pot10|…
         price_ttc, price_pro_ttc, vat_rate, avg_cost, alert_threshold, active)

locations(id, name_fr, name_ar, kind, active)                                -- serre|parcelle|vente|depot
stock(variant_id, location_id, qty, PRIMARY KEY(variant_id, location_id))
stock_moves(id, uuid, variant_id, kind,                                      -- in|out|transfer|adjust
            from_location_id, to_location_id, qty, reason, loss_reason,
            ref_kind, ref_id,                                                -- sale|purchase|inventory|batch
            note, user_id, device_id, created_at, synced_at)
inventories(id, uuid, location_id, status, started_by, started_at, validated_by, validated_at)
inventory_lines(inventory_id, variant_id, qty_expected, qty_counted)

customers(id, kind, name, phone, phone2, email, address, tax_id, lang,
          credit_limit, notes, active)
sales(id, uuid, number, customer_id, status, subtotal, discount, vat_total,
      total, paid_total, user_id, device_id, created_at, cancelled_at, cancel_reason)
sale_lines(id, sale_id, variant_id, label_snapshot, qty, unit_price, vat_rate,
           discount_bp, line_total)
payments(id, uuid, sale_id, customer_id, method,                             -- cash|cheque|transfer
         amount, cheque_number, cheque_bank, cheque_due, user_id, created_at)

quotes(id, number, customer_id, status, valid_until, …)  quote_lines(…)
invoices(id, number, kind,                                                   -- invoice|credit_note
         sale_id, customer_id, totals…, stamp_duty, issued_at, pdf_path)

suppliers(id, name, phone, email, tax_id, payment_terms, notes, active)
purchase_orders(id, number, supplier_id, status, ordered_at, …)  po_lines(…)
receipts(id, po_id, supplier_id, location_id, received_at, user_id)  receipt_lines(…)

batches(id, number, product_id, origin, qty_initial, location_id,
        started_at, status, notes)                                           -- growing|sellable|closed
batch_events(id, uuid, batch_id, kind,                                       -- repot|move|loss|sellable|treatment
             qty, variant_id, to_location_id, loss_reason, product_used, dose,
             note, user_id, device_id, created_at)

settings(key, value)                    -- JSON par clé : société, TVA, timbre, plafonds…
doc_counters(kind, year, next_number)   -- numérotations T-/F-/AV-/L-
audit_log(id, user_id, device_id, entity, entity_id, action, details_json, created_at)
oplog(seq, uuid, entity, entity_id, action, payload_json, device_id, created_at)  -- flux de sync
schema_migrations(version, applied_at)
```

### 3.2 Choix structurants

- **UUID + id local** : les entités créées hors-ligne (mobile) portent un `uuid` (v4) comme identité de sync ; l'`id` entier reste la clé interne du poste.
- **Snapshots** : `sale_lines.label_snapshot` et `unit_price` figent le libellé/prix au moment de la vente (les fiches produits évoluent).
- **Numérotation sans trou** (RG-04.b) : `doc_counters` incrémenté dans la même transaction que l'insertion du document — et uniquement sur le poste serveur (les ventes ne se font pas sur mobile, ce qui évite les numéros hors-ligne).
- **Migrations** : fichiers SQL versionnés embarqués (`:/migrations/00N_*.sql`), appliqués au démarrage dans une transaction ; `PRAGMA user_version` en garde-fou.
- **SQLite** : `journal_mode=WAL`, `foreign_keys=ON`, `busy_timeout=5000`. Accès via un thread d'E/S dédié par connexion (jamais de SQL dans le thread UI).
- **Photos** : sur disque (`data/photos/{uuid}.jpg`, redimensionnées max 1600 px, EXIF nettoyé), la base ne stocke que le chemin ; transfert via l'API de sync.

---

## 4. Serveur LAN & API

Le poste principal expose une API REST JSON sur le LAN (port par défaut **8477**, configurable), servie par `QHttpServer` dans un thread dédié.

### 4.1 Endpoints (v1)

```
POST /api/v1/pair                  # appairage : code affiché à l'écran → jeton d'appareil
POST /api/v1/auth                  # login user (PIN) → jeton de session (expirant)
GET  /api/v1/ping                  # découverte / santé / version de schéma

GET  /api/v1/sync/changes?since=SEQ        # delta oplog (pagination par seq)
POST /api/v1/sync/push                     # lot d'opérations du client (idempotent par uuid)
GET  /api/v1/photos/{uuid}                 # binaire photo
POST /api/v1/photos                        # upload photo

GET  /api/v1/dashboard             # mini-dashboard mobile (lecture seule, rôle Gérant)
```

### 4.2 Découverte & appairage

- Le serveur s'annonce en **mDNS/Zeroconf** (`_nursera._tcp`) ; repli : saisie IP manuelle.
- Appairage (F09-09) : le desktop affiche un QR `{ip, port, code éphémère}` ; le mobile le scanne, appelle `/pair`, reçoit un **jeton d'appareil** (aléatoire 256 bits, haché en base). Révocable depuis l'administration (F11-05).
- Toutes les requêtes portent `Authorization: Bearer <jeton>` ; les écritures portent aussi l'utilisateur authentifié.
- Transport : HTTP sur LAN en V1 (menace faible, réseau privé) ; TLS avec certificat auto-généré épinglé à l'appairage en V1.5.

---

## 5. Synchronisation offline-first (mobile)

### 5.1 Principe

- Le mobile possède sa **propre base SQLite** (schéma identique, sous-ensemble des tables).
- **Pull** : réplication du référentiel (produits, variantes, stock, emplacements, lots) via `GET /sync/changes?since=seq` — le mobile mémorise le dernier `seq` appliqué.
- **Push** : les saisies mobiles (mouvements, lignes d'inventaire, événements de lot, photos) sont journalisées localement en `pending_ops` puis envoyées par lots ; le serveur acquitte op par op (idempotence par `uuid` — un renvoi après coupure ne duplique rien).
- Déclencheurs : au retour du réseau (détection `QNetworkInformation`), toutes les 5 min en présence de réseau, et bouton manuel (F09-01).

### 5.2 Résolution de conflits

| Cas | Règle |
|---|---|
| Mouvement de stock mobile vs autres mouvements | **Pas de conflit** : les mouvements sont additifs, le stock est la somme des mouvements ; l'ordre n'importe pas. |
| Deux inventaires du même emplacement qui se chevauchent | Le 2e validé est mis en file **« Conflits à arbitrer »** (RG-09.b) : le Gérant choisit d'appliquer, fusionner ou rejeter. |
| Édition concurrente d'une fiche (rare : mobile ne modifie pas les fiches en V1) | Last-write-wins champ par champ + trace dans `audit_log`. |
| Vente d'un produit pendant qu'un transfert mobile est en attente | Stock potentiellement négatif → signalement (RG-03.b), jamais de blocage. |

**Choix assumé** : modéliser un maximum d'écritures comme des **faits additifs immuables** (mouvements, événements, paiements) plutôt que des états modifiables — la sync devient triviale et auditable.

### 5.3 Multi-postes desktop

Les postes desktop secondaires (caisse) travaillent **en ligne** contre l'API du poste principal (même API que le mobile, sans cache offline en V1) ; si le poste principal est éteint, la caisse affiche un message clair « Poste principal injoignable ». Un mode offline caisse est à l'étude pour V2.

---

## 6. Internationalisation & RTL

- **Qt Linguist** : `tr()` côté C++, `qsTr()` côté QML ; fichiers `nursera_fr.ts`, `nursera_ar.ts` ; `lupdate`/`lrelease` intégrés au build CMake (`qt_add_translations` — mécanisme déjà en place dans GestionScolaire avec `i18n/ar_AE.ts`).
- Changement de langue **à chaud** (F11-03) : `QQmlEngine::retranslate()` après rechargement du `QTranslator`.
- **RTL** : `LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft` + `LayoutMirroring.childrenInherit: true` posés à la racine des fenêtres. Interdiction des ancres `left/right` codées en dur dans les composants métier — utiliser exclusivement `Layout`/`anchors` symétriques et `leading/trailing` sémantiques (voir doc UX §5).
- Données bilingues (RT-03) : helper C++ `LocalizedText::pick(fr, ar, locale)` avec repli.
- Nombres/dates : `QLocale("fr_TN")` / `QLocale("ar_TN")` ; montants toujours via `Money::toDisplayString`.
- **Montant en lettres** (F05-03) : module dédié `common/amount_to_words_{fr,ar}.cpp` + tests unitaires exhaustifs.

---

## 7. Impression & documents

- **Factures / devis A4** : gabarits HTML/CSS internes rendus via `QTextDocument::print()` vers `QPrinter`/`QPdfWriter` — gabarit FR (LTR) et AR (RTL) distincts, logo société injecté.
- **Tickets 80 mm** : `QPrinter` avec `pageSize` personnalisé ; compatibilité imprimantes ESC/POS via le driver Windows en V1 (pas de protocole ESC/POS direct avant V1.5).
- **Étiquettes QR** (F02-07) : génération QR par ZXing-C++, planches A4 (gabarit 3×8) ou rouleau.
- Polices embarquées dans les ressources pour un rendu identique partout : latin + arabe (voir doc UX §3.2).

---

## 8. Sécurité & fiabilité

| Sujet | Mesure |
|---|---|
| Secrets utilisateurs | PIN/MDP hachés **Argon2id** (mémoire 64 Mo, itérations calibrées ~250 ms) ; jamais en clair, jamais loggés |
| Jetons appareils | Aléatoire CSPRNG 256 bits, seul le hash est stocké ; révocation immédiate côté serveur |
| Anti-bruteforce PIN | Compteur d'échecs + verrouillage (F01-05), délai progressif côté serveur |
| Audit | `audit_log` sur toute écriture sensible (M01-F01-06) |
| Sauvegardes | `VACUUM INTO` quotidien vers `backups/nursera-AAAAMMJJ.db` + copie sur chemin secondaire (clé USB/NAS), rotation 30 j, **test d'intégrité** (`PRAGMA integrity_check`) après chaque backup ; restauration guidée depuis l'app |
| Crash-safety | WAL + transactions ; brouillon de panier de caisse persisté à chaque modification (RT-06) |
| Logs | Journal applicatif rotatif (`logs/`, 14 j) via `QLoggingCategory` ; aucun montant client ni secret dans les logs |
| Mises à jour | Vérification de compatibilité de version de schéma entre client et serveur au `ping` ; message clair si mise à jour requise |

---

## 9. Arborescence du dépôt & build

Transposition directe des conventions de GestionScolaire (libs statiques `Core`/`Services`, modules QML `components`/`pages`, `qt_add_translations`, versionnage git-tag) à un projet bi-cible :

```
plant_nursery/
├── CMakeLists.txt                 # superbuild : version via tag git v*, politiques QTP0001/QTP0004
├── cmake/                         # fonctions (déploiement, traductions)
├── apps/
│   ├── desktop/                   # main.cpp, app_controller, qml/ (fenêtres desktop)
│   └── mobile/                    # main.cpp, qml/ (écrans Android), android/ (manifest, icônes — cf. ftour_jomaa)
├── libs/
│   ├── core/          (statique)  # src/common (result.h, money.h, enums.h), src/models,
│   │                              # src/repositories (I* + sqlite/), src/database (manager + worker),
│   │                              # migrations/ (SQL versionnés)
│   ├── services/      (statique)  # src/services + src/controllers (pattern GS_Services)
│   ├── sync/          (statique)  # HttpServer, SyncClient, oplog
│   └── ui-common/     (module QML)# thème NTheme, composants N* (équivalent GestionScolaire_Components)
├── qml/                           # modules QML pages par app si besoin (pattern GestionScolaire_Pages)
├── fonts/                         # Inter (reprendre les TTF de GestionScolaire) + Cairo
├── i18n/                          # nursera_fr.ts, nursera_ar.ts (qt_add_translations)
├── resources/                     # logo (logo.jpeg + déclinaisons PNG), gabarits documents
├── tests/
│   ├── unit/                      # Qt Test C++ (Money, TVA, montant en lettres, sync, repos)
│   └── qml/                       # TestCase QML (composants)
├── tools/                         # scripts : seed de démo, import CSV
├── build_installer.ps1            # même approche que GestionScolaire
├── installer.iss                  # Inno Setup
└── docs/                          # ces documents
```

- **Presets CMake** : `desktop-debug`, `desktop-release`, `android-release` (`CMakePresets.json`) — correspondant aux kits Qt Creator déjà configurés (`Desktop_Qt_6_11_0_MinGW_64_bit`, `Qt_6_11_0_pour_Android_arm64_v8a`).
- **MinGW** : pas de `/utf-8` MSVC ; sources en UTF-8 par défaut — conserver le garde `if(MSVC)` de GestionScolaire pour la portabilité.
- **Qualité** : `clang-format` + `clang-tidy` (config au dépôt), warnings-as-errors sur le code projet, `qmllint` (déjà actif dans le build GestionScolaire).
- **CI (recommandé)** : build desktop + Android, exécution des tests unitaires, `lupdate` en contrôle (pas de chaîne non traduite).

### Packaging & déploiement

| Cible | Outil | Livrable |
|---|---|---|
| Windows | `windeployqt` + **Inno Setup** via `build_installer.ps1` (chaîne reprise de GestionScolaire : `installer.iss`, dossier `deploy/` avec redistribuables) | Installateur `.exe` (app + Qt dynamique + MinGW runtime), mise à jour = réinstallation par-dessus (les données dans `%APPDATA%/Nursera` sont préservées) |
| Android | Qt 6.11 for Android (Gradle — chaîne validée par `ftour_jomaa`) | APK signé `arm64-v8a`, distribution directe (pas de Play Store en V1) ; installation via lien local ou câble |

Données desktop : `%APPDATA%/Nursera/` → `nursera.db`, `photos/`, `backups/`, `logs/`, `settings`.

---

## 10. Exigences non fonctionnelles

| Exigence | Cible |
|---|---|
| Démarrage à froid desktop | < 3 s |
| Démarrage à froid Android (milieu de gamme) | < 4 s |
| Recherche produit (10 000 variantes) | < 100 ms (index FTS5 sur noms FR/AR/latin) |
| Sync mobile complète initiale (5 000 produits + photos vignettes) | < 2 min en Wi-Fi |
| Sync incrémentale | < 5 s |
| Empreinte APK | < 60 Mo |
| Volume base à 5 ans (estimation) | < 500 Mo hors photos — confortable pour SQLite |
| Postes simultanés | 5 clients LAN sans dégradation |

---

## 11. Risques techniques & parades

| Risque | Impact | Parade |
|---|---|---|
| Rendu arabe (ligatures, chiffres) incohérent selon plateformes | UX AR dégradée | Polices embarquées + revue systématique de chaque écran en AR dès le MVP (pas « à la fin ») |
| Poste principal éteint → caisse bloquée | Vente impossible | Message explicite + procédure papier de secours ; mode offline caisse étudié en V2 |
| Qualité du Wi-Fi dans les serres | Sync retardée | Offline-first natif ; l'app n'exige jamais le réseau pour saisir |
| Dérive du schéma entre versions client/serveur | Corruption de sync | Version de schéma dans `/ping`, refus de sync si incompatible, migrations testées |
| Imprimantes tickets hétérogènes | Impression KO | V1 via driver Windows uniquement ; liste d'imprimantes validées fournie au client |
| ZXing/caméra sur Android bas de gamme | Scan lent | Saisie manuelle toujours possible en parallèle du scan |
