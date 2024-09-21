# 🌱 Nursera — Vision produit & Grands axes

> **Application de gestion pour Pépinière Idéale** (*Vente, Aménagement & Entretien*) — Tunisie
> Desktop (Windows) + Android · C++ / QML / Qt 6.11
> Logo : [logo.jpeg](../logo.jpeg) (fourni) · Version du document : 1.1 · Date : 14/07/2026 · Statut : Validé (cadrage initial)
> « Nursera » est le nom de code technique du projet ; le produit porte la marque **Pépinière Idéale**.

---

## 1. Contexte & problème

Une petite pépinière tunisienne gère aujourd'hui son activité manuellement (cahiers, mémoire, téléphone) :

- **Stock invisible** : personne ne sait précisément combien de plants sont disponibles, dans quelle serre, à quel stade de croissance.
- **Ventes non tracées** : pas d'historique fiable des ventes, des remises accordées, des créances clients.
- **Pertes non mesurées** : la mortalité des plants (sécheresse, maladie, gel) n'est ni enregistrée ni analysée.
- **Aucun pilotage** : impossible de savoir quels produits sont rentables, quelles saisons performent, quels clients sont fidèles.

## 2. Vision

> **« Le carnet de la pépinière, dans la poche et au bureau. »**

Une application **simple, bilingue (FR/AR), fonctionnant sans Internet**, qui devient l'outil de travail quotidien de la pépinière :

- Le **desktop** au bureau : gestion, caisse, facturation, rapports.
- L'**Android** sur le terrain : consultation du stock, inventaires, mouvements, photos — dans les serres et les parcelles.
- Les données restent **chez l'entreprise** (réseau local), sans abonnement cloud ni dépendance externe.

## 3. Utilisateurs cibles (personas)

| Persona | Profil | Besoins clés | Appareil principal |
|---|---|---|---|
| **Sami — Gérant** | Propriétaire, 45 ans, gère tout, à l'aise en français | Vue d'ensemble, marges, créances, décisions d'achat/production | Desktop + Android |
| **Leïla — Vendeuse** | Au point de vente, contact client, bilingue FR/AR | Encaisser vite, vérifier un prix/stock, créer un devis | Desktop (caisse) |
| **Khaled — Ouvrier de serre** | Sur le terrain, préfère l'arabe, peu à l'aise avec l'informatique | Déclarer un rempotage, des pertes, compter un inventaire — en 3 taps | Android |

**Implication UX majeure** : l'interface Android doit être utilisable **avec des gants, au soleil, par un utilisateur peu technophile, en arabe**. Simplicité radicale.

## 4. Grands axes du produit

### Axe 1 — Catalogue & Stock 📦
Le référentiel central : fiches plantes bilingues (nom commun FR/AR, nom botanique), catégories, conditionnements (godet, pot, motte, racines nues), prix, photos. Stock multi-emplacements (serres, parcelles, zone de vente) avec mouvements tracés et alertes de seuil.

### Axe 2 — Ventes & Caisse 💰
Vente comptoir rapide (ticket), devis, factures conformes (TND à 3 décimales, TVA configurable, timbre fiscal), remises, paiements espèces/chèque/virement, gestion des créances clients.

### Axe 3 — Tiers & Achats 🤝
Clients (particuliers, professionnels : paysagistes, communes, hôtels) avec historique et encours. Fournisseurs, commandes d'achat, réceptions qui alimentent le stock.

### Axe 4 — Production & Terrain 🌿
Suivi des lots de production : semis, bouturage, rempotage, traitements phytosanitaires, mortalité. L'app Android est l'outil de saisie terrain : inventaires, mouvements, photos, étiquettes QR.

### Axe 5 — Pilotage & Rapports 📊
Tableau de bord (ventes du jour/mois, stock critique, créances), rapports ventes/stock/pertes/marges, export PDF et CSV.

### Axe 6 — Administration & Socle ⚙️
Utilisateurs et rôles (Gérant, Vendeur, Ouvrier), bilinguisme FR/AR avec RTL, sauvegardes automatiques, paramètres société (logo — déjà disponible, coordonnées, taux TVA, timbre fiscal), synchronisation LAN desktop ↔ Android.

## 5. Répartition des rôles par plateforme

| Fonction | Desktop | Android (terrain) |
|---|:---:|:---:|
| Catalogue (création/édition) | ✅ | 👁 consultation |
| Stock — consultation | ✅ | ✅ |
| Stock — mouvements & inventaire | ✅ | ✅ (cœur de l'app) |
| Ventes, caisse, factures | ✅ | ❌ |
| Clients / Fournisseurs / Achats | ✅ | ❌ |
| Production (lots, pertes, traitements) | ✅ | ✅ saisie rapide |
| Photos des plants | — | ✅ |
| Scan QR / code-barres | via douchette | ✅ caméra |
| Rapports & tableau de bord | ✅ | 👁 mini-dashboard |
| Administration | ✅ | ❌ |

## 6. Architecture de déploiement (résumé)

```
        ┌────────────────────── Réseau local (Wi-Fi) ─────────────────────┐
        │                                                                  │
  ┌─────┴──────┐  poste principal      ┌────────────┐      ┌─────────────┐
  │  PC Bureau │  = serveur de données │ PC Caisse  │      │  Android    │
  │  (Gérant)  │  SQLite + service     │ (Vendeuse) │      │  (Terrain)  │
  │            │  de sync LAN          │  client    │      │  offline-   │
  └────────────┘                       └────────────┘      │  first+sync │
                                                           └─────────────┘
```

- **Pas d'Internet requis** pour fonctionner.
- Le poste principal héberge la base ; les autres appareils s'y connectent en LAN.
- L'Android fonctionne **hors connexion** (cache local) et se synchronise dès qu'il retrouve le Wi-Fi.

## 7. Roadmap proposée

### 🎯 MVP (V0.9) — « On remplace le cahier » (~3 mois)
- Socle : auth, rôles, FR/AR, paramètres société, sauvegardes
- Catalogue plantes + photos
- Stock mono/multi-emplacements, mouvements, alertes
- Vente comptoir + ticket + journal des ventes
- Clients (fiche + créances simples)
- Android : consultation stock + inventaire + sync LAN

### 🚀 V1.0 — « On gère l'entreprise » (+2 mois)
- Devis & factures PDF (timbre fiscal, mentions légales)
- Fournisseurs + achats + réceptions
- Production : lots, rempotage, pertes
- Rapports complets + tableau de bord
- Étiquettes QR imprimables + scan Android

### 🌟 V1.5 — « On optimise » (+2 mois)
- Traitements phytosanitaires + rappels d'entretien
- Analyse des marges et de la mortalité par lot
- Multi-douchettes, impression tickets 80 mm
- Export comptable

### 🔮 V2.0 — pistes (non engagées)
- **Module chantiers/interventions** : l'activité *Aménagement & Entretien* du logo suggère des prestations de service (création de jardins, contrats d'entretien). En V1, elles sont facturables via les lignes libres des devis/factures (F05-01) ; un vrai module (planification des interventions, suivi de chantier, contrats récurrents) est une extension naturelle — **à valider avec le gérant : quelle part du CA représentent ces services ?**
- Catalogue client (vitrine Android/web, réservations)
- Multi-sites via serveur distant
- Prévisions de production basées sur l'historique

## 8. Indicateurs de succès (KPI produit)

| KPI | Cible à 6 mois |
|---|---|
| Écart inventaire physique vs application | < 5 % |
| Temps de saisie d'une vente comptoir | < 30 s |
| Ventes enregistrées dans l'app | 100 % |
| Taux de mortalité mesuré (vs non mesuré aujourd'hui) | mesuré sur 100 % des lots |
| Utilisation quotidienne de l'app terrain par les ouvriers | ≥ 1 saisie/jour/ouvrier |

## 9. Contraintes & hypothèses

- **Budget PME** : pas d'abonnement cloud, pas de serveur dédié — le PC du bureau fait office de serveur.
- **Connectivité** : Internet instable/absent ; le Wi-Fi local couvre le bureau, pas forcément toutes les serres → **offline-first sur Android obligatoire**.
- **Matériel** : PC Windows existants, smartphones/tablettes Android milieu de gamme (Android 9+), imprimante A4 existante, imprimante ticket 80 mm optionnelle.
- **Réglementaire (Tunisie)** : TND à 3 décimales (millimes), TVA multi-taux configurable (0 / 7 / 13 / 19 %), timbre fiscal configurable sur factures, mentions matricule fiscal.
- **Licences** : Qt en LGPLv3 (liaison dynamique) → coût de licence nul.

## 10. Documents liés

| Document | Contenu |
|---|---|
| [01-specifications-fonctionnelles.md](01-specifications-fonctionnelles.md) | Spécifications détaillées par module : user stories, règles de gestion, permissions |
| [02-specifications-techniques.md](02-specifications-techniques.md) | Architecture C++/QML/Qt, modèle de données, synchronisation, sécurité, packaging |
| [03-ux-ui-design.md](03-ux-ui-design.md) | Charte UX/UI, design system, navigation, wireframes, RTL |
