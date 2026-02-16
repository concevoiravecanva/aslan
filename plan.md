# Plan : Interface Data-Driven Design — Splitter 3 états

## Concept

Transformer l'interface actuelle (blocs empilés verticalement) en un **layout split gauche/droite** :
- **Gauche ("Cerveau")** : Master Table — source de données
- **Droite ("Caméléon")** : Vue active avec onglets (Design, Canvas, Kanban, Calendrier, Graphique)

Un **splitter vertical draggable** à 3 modes : 50/50 (collaboration), 3/97 (design full), 97/3 (data full).

---

## Architecture — Ce qui change vs. ce qui reste

### Ce qui RESTE intact
- Tout le code de rendu : `ui_render_block_table()`, `ui_render_block_canvas()`, `ui_render_block_design()`, `ui_render_block_chart()`
- Le système de noeuds, la sidebar properties/layers, les toolbars
- Les popups (date picker, select, column settings)
- Le drag-and-drop de blocs (s'appliquera au sein de chaque panneau)
- Le document/page/block model existant

### Ce qui CHANGE
- `ui_render()` dans `ui_host.c` : le layout principal passe de "blocs empilés" à "split view"
- Ajout d'un état `splitter` dans `UIHost`
- Ajout d'onglets de vue dans le panneau droit
- La table "master" du panneau gauche est un BLOCK_TABLE existant (le premier trouvé ou sélectionné)
- Les vues droite sont les blocs existants (CANVAS, DESIGN, CHART) + nouvelles vues Kanban/Calendar

---

## Phase 1 : Splitter vertical + Layout de base

### 1.1 — État dans UIHost (ui_host.h)
```c
// Splitter / Data-Driven layout
struct {
  float ratio;          // 0.0 à 1.0 — portion gauche (0.5 = 50/50)
  int is_dragging;      // 1 = dragging le splitter
  int mode;             // 0=collaboration(50/50), 1=design(3/97), 2=data(97/3)
  uint32_t master_table_id;  // Block ID de la table "cerveau"
  int active_view;      // 0=Design, 1=Canvas, 2=Kanban, 3=Calendar, 4=Chart
} split_view;
```

### 1.2 — Rendu split dans ui_render() (ui_host.c)
Quand `split_view.master_table_id != 0` (mode split actif) :
- Remplacer le layout bloc-empilé par un split gauche/droite
- Utiliser `nk_layout_row_template` : `push_static(left_w)` + `push_static(splitter_w=6)` + `push_variable(right)`
- Panneau gauche : `nk_group_begin("DataPanel")` → rendu de la master table
- Splitter : zone de 6px draggable (curseur resize horizontal)
- Panneau droit : `nk_group_begin("ViewPanel")` → onglets + vue active

### 1.3 — Splitter draggable
- Détection souris dans zone splitter (±4px)
- En drag : `split_view.ratio = clamp(mouse_x / win_w, 0.03, 0.97)`
- Double-clic sur splitter : cycle entre les 3 modes (50/50 → 3/97 → 97/3 → 50/50)
- Curseur `UI_CURSOR_RESIZE_H` au survol

### 1.4 — Mode 3% (ultra-compact)
Quand `ratio <= 0.05` :
- Le panneau gauche affiche seulement des pastilles de couleur par ligne (pas de texte)
- Chaque pastille = 1 ligne de la table, couleur = colonne "Select" ou premier champ coloré
- Clic sur pastille = sélectionne cette ligne dans la table et highlight dans la vue droite

---

## Phase 2 : Onglets de vue (panneau droit)

### 2.1 — Barre d'onglets supérieure
- Hauteur : 32px, fond subtil, au-dessus de la vue
- Onglets : `Design | Canvas | Kanban | Calendar | Chart`
- Style : onglet actif avec accent bleu + underline, inactifs en gris
- Icônes Remix à côté du texte de chaque onglet
- Stocké dans `split_view.active_view`

### 2.2 — Rendu conditionnel
- `active_view == 0` : cherche le premier BLOCK_DESIGN et le rend en fullscreen-like
- `active_view == 1` : cherche le premier BLOCK_CANVAS et le rend
- `active_view == 2` : vue Kanban (nouveau, Phase 4)
- `active_view == 3` : vue Calendar (nouveau, Phase 5)
- `active_view == 4` : cherche le premier BLOCK_CHART et le rend

Si le bloc correspondant n'existe pas, afficher un bouton "+" pour le créer.

---

## Phase 3 : Synchronisation Table ↔ Vue

### 3.1 — Sélection bidirectionnelle
- Clic sur une ligne de table → highlight le noeud correspondant dans canvas/design
- Clic sur un noeud dans canvas/design → highlight la ligne correspondante dans la table
- Utiliser le champ `name` du noeud comme clé de liaison (ou un mapping node_id ↔ row_id)

### 3.2 — Ajout d'un champ de liaison
Dans `UIHost.split_view` :
```c
int selected_row;     // Index de la ligne sélectionnée dans la master table
```
La vue droite utilise `selected_row` pour mettre en surbrillance l'élément correspondant.

---

## Phase 4 : Vue Kanban (nouveau)

### 4.1 — Basé sur les colonnes Select
- Identifie une colonne de type ASLAN_COL_SELECT dans la master table
- Chaque option de Select = une colonne Kanban
- Chaque ligne de la table = une carte dans la colonne correspondante
- Drag-and-drop d'une carte entre colonnes = change la valeur Select dans la table

### 4.2 — Rendu
- Colonnes horizontales avec header coloré (couleur de l'option)
- Cartes : titre (première colonne text) + badges (autres colonnes)
- Hauteur : scroll vertical par colonne si trop de cartes
- Fonction : `ui_render_view_kanban(ui, engine, ctx, table, width, height)`

---

## Phase 5 : Vue Calendrier (nouveau)

### 5.1 — Basé sur les colonnes Date
- Identifie une colonne de type ASLAN_COL_DATE dans la master table
- Chaque ligne avec une date = un événement sur le calendrier
- Grille mensuelle style Notion (réutilise le code du date picker)

### 5.2 — Rendu
- Navigation mois (< Month Year >)
- Grille 7 colonnes (Lu-Di), 6 rangées
- Événements affichés comme petites pastilles colorées dans les cellules
- Clic sur événement = sélection dans la table
- Fonction : `ui_render_view_calendar(ui, engine, ctx, table, width, height)`

---

## Phase 6 : Activation / Toggle

### 6.1 — Entrée dans le mode Split
- Depuis l'éditeur normal : clic sur un nouveau bouton dans la toolbar ("Split View" / icône)
- Ou automatiquement quand une page contient au moins 1 table + 1 bloc visuel
- `split_view.master_table_id` = ID du premier BLOCK_TABLE de la page

### 6.2 — Sortie du mode Split
- Bouton "Exit Split" ou Escape quand pas de popup
- Retour au layout bloc-empilé classique
- `split_view.master_table_id = 0`

---

## Ordre d'implémentation

| Étape | Description | Fichiers |
|-------|-------------|----------|
| 1 | Struct `split_view` dans UIHost | ui_host.h |
| 2 | Layout split + splitter draggable dans ui_render() | ui_host.c |
| 3 | Panneau gauche : rendu master table | ui_host.c |
| 4 | Barre d'onglets + dispatch vue droite | ui_host.c (nouveau fn) |
| 5 | Activation/toggle depuis toolbar | ui_core.c, ui_host.c |
| 6 | Synchronisation table ↔ vue (highlight bidirectionnel) | ui_host.c, ui_table.c, ui_canvas.c |
| 7 | Vue Kanban | ui_host.c (ou nouveau ui_kanban.c) |
| 8 | Vue Calendrier | ui_host.c (ou nouveau ui_calendar.c) |
| 9 | Mode 3% compact (pastilles) | ui_host.c |

Estimation : ~800-1200 lignes de nouveau code.
