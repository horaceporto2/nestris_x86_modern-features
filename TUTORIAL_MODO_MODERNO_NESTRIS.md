# Tutorial de implementação — Modo Moderno do `nestris_x86`

> **Público-alvo: uma IA (ou dev) que vai continuar o trabalho sem ter visto a conversa original.**
> Este documento é autossuficiente: descreve o projeto, o que já foi feito e verificado, o que ainda falta, e para cada tarefa dá objetivo, arquivos, passos, código e critério de aceite.
> Idioma do projeto/usuário: **português (Brasil)**. Responda e comente em português; identificadores de código ficam em inglês.

Legenda de confiança usada nas tarefas:
- ✅ **Verificado** — o código foi compilado e testado de forma headless (lógica de jogo, sem SDL/olc).
- ⚠️ **Não compilado** — escrito com cuidado, mas depende de arquivos que não foram vistos ou de renderização; **compile e teste visualmente**.
- ❓ **Investigar** — depende de arquivos que **não** foram enviados; leia-os antes de editar.

---

## 0. Antes de começar (leia isto)

1. O usuário deve te fornecer **o repositório inteiro** (ou, no mínimo, os arquivos listados na seção 3) **mais os 6 arquivos corrigidos** da seção 2.2. Se você só tiver este documento, peça os arquivos.
2. Nunca assuma o conteúdo de um arquivo marcado ❓. Abra, leia, só depois edite.
3. Mantenha o modo **clássico (NES) 100% inalterado**. Toda mudança de regra deve ficar atrás de `state.modern_mode` / `options.modern_mode`.
4. Depois de cada tarefa, compile (`cmake` + `make`, ver seção 1.1) e confirme que o modo clássico ainda joga igual.
5. Não renomeie funções/arquivos existentes e não reformate arquivos inteiros (diffs pequenos).

---

## 1. Contexto do projeto

**O que é:** clone em C++17 do NES Tetris ("NestrisX86") usando a *olc::PixelGameEngine* (tela lógica **256×225**, escala 4×), SDL 1.2 + SDL_mixer para som, yaml-cpp para configuração. O usuário (Carlos) está adicionando um **modo moderno** ("The Ultimate Combo"): **Hold, Hard Drop e regras modernas** (SRS, fantasma, lock delay, gravidade/pontuação guideline).

### 1.1 Build (Linux)
```bash
mkdir -p build && cd build
cmake ..            # CMakeLists.txt na raiz
make -j$(nproc)
./nestris_x86       # precisa rodar da pasta onde ficam config.yaml e assets/ se LOAD_FROM_BINARY=false
```
Alvos: `nestris_x86` (jogo), `asset_cpp_gen` (gera `src/assets_cpp/*.cpp` com sprites/sons embutidos), `sdl_gamepad`, `testbed`.
Os `.cpp` ficam em `src/` (jogo em `src/`, processadores em `src/frame_processors/`, input em `src/input_devices/`, drawers em `src/drawers/`), os `.hpp` em `include/` (o `game_processor.hpp` fica em `include/frame_processors/`).

### 1.2 Convenções que NÃO podem ser quebradas
- **Grid:** `GameState<>::Grid = std::array<std::array<int,20>,10>` → acesso **`grid[x][y]`**, x = coluna (0..9), y = linha (0 = topo, 19 = chão). `y` cresce para baixo.
- **Matrizes de peça** (`tetromino.hpp`) são **column-major**: `matrix[x][y]`.
- **Referência da peça** (`TetrominoState{tetromino, x, y, rotation, srs}`): `(x,y)` é o **centro da caixa** da peça. A origem da caixa no grid é `(x - x_offset, y - y_offset)` com `getStartOffsets()`: `{1,1}` para T/J/L/S/Z (caixa 3×3), `{2,2}` para I (caixa 4×4), `{1,0}` para o quadrado (caixa 2×2).
- **Rotação:** `+1` = horário, `-1` = anti-horário; índice vai de 0 a 3 com wrap (feito em `updateStateOnNoCollision`).
- **Flag `srs`:** `false` = orientações do NES (2 estados para S/Z/I); `true` = SRS (4 estados). Estados SRS: `0` = spawn (T apontando para cima), `1` = "R" (direita), `2` = invertido, `3` = "L" (esquerda).
- **Enum `Tetromino { T, J, Z, Square, S, L, Line }`** — a ordem importa (RNG usa `Tetromino{int}`; estatísticas usam `static_cast<Tetromino>(i)`).
- **Spawn:** NES `x=5, y=0`; moderno `x=5, y=1` (exceto o quadrado, `y=0`) para a peça inteira ficar visível em 20 linhas. Implementado em `makeSpawnState()` (namespace anônimo de `game_processor.cpp`).
- **Cores** vêm de `getColor()` (1, 2 ou 3) e o sprite do bloco depende do `level % 10`.

### 1.3 Fluxo de um frame (`GameProcessor::processFrame`)
```
processFrame(key_events)
 ├─ topped_out?  → updateTopOutState (animação da "cortina")
 ├─ paused?      → só espera Start
 ├─ entryDelay(state)  (entry_delay_counter > 0) → doEntryDelayStep
 │      animateLineClear → no frame 4 chama updateScoreAndLevel(...) e statistics_.update(...)
 └─ senão doGravityStep:
        spawn_if_needed()                 // se spawn_new_tetromino
        processKeyEvents(...)             // hold, DAS, hard drop, rotação, pausa
        spawn_if_needed()                 // spawn imediato da peça vinda do Hold
        applyGravity(...)                 // moderno → applyModernGravity (lock delay)
        se travou: som, press_down_lock, addPressDownScore,
                   checkForLineClears → updateEntryDelayForLineClear + LineClearAnimationInfo
 renderer_.renderGameState(...)
```
Pontos importantes:
- **A pontuação de linhas só é somada no frame 4 da animação** (`updateScoreAndLevel`), não no instante do lock. Qualquer bônus sem linhas (ex.: T-spin sem linha) precisa ser somado no lock.
- No modo moderno o `entry_delay_counter` é 0 ao travar (sem ARE); só existe atraso quando há linhas completas (animação).
- Durante o atraso/animação **nenhuma tecla é processada** (nem Hold).
- `GameState` é um *template* `GameState<W=10,H=20>` com construtor de lista de inicialização manual — **todo membro novo deve ser inicializado nessa lista**.

---

## 2. Estado atual

### 2.1 Já implementado (e confirmado por testes headless ✅)
| Recurso | Onde | Observação |
|---|---|---|
| Orientações SRS das 7 peças (4 estados) | `tetromino.hpp` `getSrsTetrominoGrid` | todas com 4 blocos; spawn nas linhas 0–1 |
| Rotação SRS + tabelas de wall kick JLSTZ e I | `game_logic.cpp` `rotateTetrominoSrs` | tabelas conferidas contra o padrão oficial; O não gira |
| Hold | `game_logic.cpp` `holdTetromino` + `processKeyEvents` | 1 por peça; 1º hold puxa a próxima da fila; troca não consome a fila |
| Hard drop | `processKeyEvents` | 2 pts/célula; trava no mesmo frame; processado **depois** de mover/girar |
| Soft drop moderno | `processKeyEvents` + `applyModernGravity` | 1 frame/linha, 1 pt/célula, pontos somados na hora |
| Fantasma | `getGhostTetromino` + `GameRenderer::renderGhost` | contorno cinza; não desenhado durante spawn pendente/animação |
| Lock delay | `applyModernGravity` | 30 frames, "move reset" até 15 resets, refresca ao descer para nova linha mais baixa |
| Gravidade guideline | `modernGravityFrames` | `(0.8-(L-1)*0.007)^(L-1)` s/linha, mínimo 1 frame |
| Pontuação moderna de linhas | `getModernScoreForLineClear` | 100/300/500/800 × nível |
| Som de line clear/tetris no moderno | `updateEntryDelayForLineClear(…, modern)` | delay ≥ 24 frames |
| Tecla Hold (teclado `C`, gamepad `FACE_U`) | `key_defines.hpp` | merge com defaults no `nestris_x86.cpp` |
| Caixa HOLD na tela | `GameRenderer::renderHold` | posição em y=10..50 (acima do painel de estatísticas, que começa em y=61) |
| Campo `modern_mode` em `GameOptions` e declarações do renderer | `game_processor.hpp`, `game_renderer.hpp` | corrige os erros de compilação originais |

### 2.2 Os 6 arquivos corrigidos que o usuário recebeu (precisam estar no projeto)
`game_processor.hpp`, `game_processor.cpp`, `game_renderer.hpp`, `game_renderer.cpp`, `game_logic.hpp`, `game_logic.cpp`.
Se **não** estiverem aplicados, a tarefa A1 abaixo os recria a partir deste resumo de mudanças em relação ao código original do usuário:

1. `GameOptions` ganha `bool modern_mode{false};`.
2. `game_renderer.hpp` declara `renderGhost(const GameState<>&, const GameState<>::Grid&, const PixelDrawingInterface::Coords&) const` e `renderHold(const GameState<>&) const` (privados).
3. `GameRenderer::renderGameState`: ghost só se `not entryDelay(state) && not state.spawn_new_tetromino`.
4. `renderHold`: constantes `label_x=20, label_y=12, start_x=28, start_y=24`, `fillRect(label_x-4, label_y-2, 48, 40, BLACK)`.
5. `game_logic.cpp`:
   - `holdTetromino` e `lockActiveTetromino` (modo moderno) fazem `state.gravity_counter = modernGravityFrames(state.level);`
   - `applyModernGravity`: `++state.score;` (em vez de `++state.press_down_counter`) quando Down está pressionado e a peça cai.
   - `rotateTetrominoSrs` retorna `false` para o quadrado.
   - O bloco do **hard drop** em `processKeyEvents` foi movido para depois da rotação (logo antes do `Start`).
   - `updateEntryDelayForLineClear(int&, const bool modern=false)`: se `modern`, `delay_counter = max(delay_counter, 24 + rand()%5)`.
6. `game_processor.cpp`: `updateEntryDelayForLineClear(state_.entry_delay_counter, state_.modern_mode)` e `state_.press_down_lock = not state_.modern_mode;`.

### 2.3 O que falta (resumo)
| ID | Tarefa | Prioridade | Confiança |
|---|---|---|---|
| A1 | Aplicar/validar os 6 arquivos corrigidos e compilar | **Obrigatória** | ✅ |
| A2 | Registrar a opção `modern_mode` no menu de opções | **Obrigatória** (sem ela o modo nunca liga) | ❓ |
| A3 | Hold na configuração de teclas/gamepad e bindings válidos | **Obrigatória** | ❓ |
| A4 | Ajuste visual da caixa HOLD | Alta | ⚠️ |
| B1 | T-spin, back-to-back, combo, perfect clear + mensagem na tela | Alta | ✅ lógica / ⚠️ render |
| B2 | RNG 7-bag no modo moderno | Média | ❓ |
| B3 | Estatísticas (TRT) corretas no moderno | Baixa | ❓ |
| B4 | High scores separados por modo | Média | ⚠️ |
| C1 | Preview de várias próximas peças | Opcional | ⚠️ |
| C2 | IHS/IRS (hold/rotação durante a animação) | Opcional | ⚠️ |
| C3 | Extras: toggle do fantasma, som próprio de Hold, Hold no desenho do controle | Opcional | ❓ |
| C4 | Testes automatizados headless | Recomendado | ✅ |

---

## 3. Arquivos que você precisa LER antes (não foram vistos até agora) ❓
- `src/frame_processors/option_screen_processor.cpp`, `include/option.hpp` (+ `src/option.cpp` se existir) — menu de opções.
- `src/frame_processors/keyboard_config_processor.cpp` + `.hpp` — tela de teclas (também usada para o gamepad).
- `src/input_devices/olc_keyboard.cpp`, `src/input_devices/sdl_gamepad.cpp`, `include/input_devices/input_interface.hpp` — `lookupKeyCode("C")` e `lookupKeyCode("FACE_U")`.
- `include/tetromino_rng.hpp` — enum `RngType`, `tetrominoRngFactory`, `rngTypeFromString`.
- `include/statistics.hpp`, `include/das.hpp`, `include/gravity.hpp`, `include/frame_processor_interface.hpp`, `include/drawers/pixel_drawing_interface.hpp` (assinaturas de `drawString`, `fillRect`, `Coords`, `Color`).
- `src/frame_processors/level_screen_processor.cpp` (tela de nível e high score).
- O sprite de fundo `assets/images/basic-field-empty-black.png` (para ajustar posições na tela).

---

## 4. Tarefas

### A1 — Aplicar e compilar os arquivos corrigidos ✅
**Objetivo:** projeto compilando sem erros.

Passos:
1. Copie os 6 arquivos para: `include/game_processor.hpp` → `include/frame_processors/game_processor.hpp`; `include/game_renderer.hpp`; `include/game_logic.hpp`; `src/frame_processors/game_processor.cpp`; `src/game_renderer.cpp`; `src/game_logic.cpp`.
2. `cd build && cmake .. && make -j$(nproc)`.
3. Se algum erro restar, é quase certamente de declaração (header vs. `.cpp`). A assinatura precisa ser **idêntica**, incluindo `const` no fim do método.

Critério de aceite: `make` termina sem erros; o jogo abre e o modo clássico joga como antes.

---

### A2 — Registrar a opção `modern_mode` no menu ❓
**Objetivo:** o jogador consegue ligar/desligar o modo moderno no menu de opções, e a escolha persiste no `config.yaml`.

Contexto: `nestris_x86.cpp::menuOptionsToGameOptions` faz
```cpp
if (const auto it = option_map.find("modern_mode"); it != option_map.end()) {
  options.modern_mode = getBoolOption(*it->second);
}
```
Ou seja, **sem uma opção com a chave exata `"modern_mode"` o modo moderno nunca é ativado** (e o jogo compila/roda normalmente, o que esconde o problema).

Passos:
1. `grep -rn "hard_drop\|wall_kick" src include` para achar onde as opções booleanas existentes são criadas (provavelmente no construtor de `OptionScreenProcessor`, via `options_["hard_drop"] = std::make_unique<...Option>(...)`) e onde são ordenadas (`option_order_`).
2. Crie `modern_mode` **copiando exatamente o padrão de `hard_drop`**: mesma classe de opção booleana, valor padrão `false` ("OFF"), rótulo curto (a tela é 256 px de largura, fonte 8 px: ~14–16 caracteres úteis; ex.: `"MODERN"`).
3. Insira a chave no `option_order_` (sugestão: logo antes de `hard_drop`/`wall_kick`) e, se existir, no conjunto de `spacers` usado por `renderOptions`.
4. **Persistência:** confira `getOptionsAsYaml()` / `setOptionsYaml()`. Eles provavelmente iteram `options_`. Garanta que um **`config.yaml` antigo sem a chave `modern_mode` não lance exceção** nem ignore o restante (o construtor de `NestrisX86` chama `setOptionsYaml` sem `try/catch`). Teste apagando só a linha `modern_mode` do yaml.
5. **Altura da tela:** conte as linhas: o menu deve caber em 225 px. Se não couber, ajuste `first_row`/espaçamento em `renderOptions` ou remova um spacer.
6. (Opcional, recomendado) Quando `modern_mode` estiver ON, as opções `hard_drop` e `wall_kick` não têm efeito (o moderno usa hard drop próprio e SRS). Deixe-as visualmente cinza (há um mecanismo `grey_out_das_options_` — reutilize o padrão) ou apenas ignore.

Critério de aceite:
- Menu mostra a opção; alternar e iniciar o jogo no modo moderno mostra o **fantasma** e a caixa **HOLD**; no clássico não aparecem.
- Fechar e reabrir o jogo mantém a escolha (`config.yaml` contém `modern_mode`).
- `config.yaml` antigo (sem a chave) carrega sem erro.

---

### A3 — Hold na configuração de teclas e gamepad ❓
**Objetivo:** a ação `Hold` aparece e é reconfigurável na tela de teclas do teclado e do gamepad, e os códigos padrão são válidos.

Contexto: `KeyAction` agora tem 8 ações (`Up, Down, Left, Right, RotateClockwise, RotateAntiClockwise, Start, Hold`, depois `COUNT`). Padrões: teclado `C`, gamepad `FACE_U`. `nestris_x86.cpp` já faz *merge* dos bindings salvos sobre os padrões, então `config.yaml` antigo (sem Hold) funciona.

Passos:
1. Leia `keyboard_config_processor.cpp`: veja se ele itera `key_action_size`/`action_names` ou o `std::map` de bindings. Garanta que **Hold aparece** e que a lista de **8 linhas cabe** na tela (225 px). Ajuste espaçamento se necessário.
2. Leia `olc_keyboard.cpp` e `sdl_gamepad.cpp`: confirme que `lookupKeyCode("C")` e `lookupKeyCode("FACE_U")` retornam códigos válidos (não um valor de "inexistente"). Se a tabela de nomes não tiver `"C"` ou `"FACE_U"`, adicione (ou troque o padrão em `key_defines.hpp` por um nome existente).
3. Conferir o fluxo de salvar: `processProgramFlowSignal` (`OptionsScreen`) copia `getKeyBindings()` de volta para `keyboard_key_bindings_`/`gamepad_key_bindings_`. Verifique que o `Hold` reconfigurado sobrevive a esse caminho e chega a `getKeyEvents()` (que faz `gamepad_key_bindings_.at(action)` — **lança `std::out_of_range` se faltar uma ação** em qualquer um dos dois mapas).

Critério de aceite: remapear Hold para outra tecla, iniciar o jogo no modo moderno e usar a nova tecla funciona; nenhuma exceção em `getKeyEvents`.

---

### A4 — Ajuste visual da caixa HOLD ⚠️
**Objetivo:** a caixa HOLD fica alinhada com a arte do fundo e não sobrescreve o painel de estatísticas.

Contexto: não se viu o PNG do fundo. As constantes em `GameRenderer::renderHold` são palpites: `label_x=20, label_y=12, start_x=28, start_y=24`; a área apagada é `fillRect(label_x-4, label_y-2, 48, 40, BLACK)` (y de 10 a 50). O painel de estatísticas NES começa em y=61 (`counter_sprite_pos{13,61}`).

Passos: rode o jogo no modo moderno, faça Hold e olhe o resultado. Ajuste só as constantes. Peças são desenhadas com `renderGrid(x, y, getTetrominoGrid(held, 0), level, 8, 8, getColor(held))` e os mesmos deslocamentos especiais da caixa NEXT (`Square: +4,+8`; `Line: -4,-4`). Opcional: desenhar uma moldura (4 `fillRect` finos) para parecer uma caixa.

Critério de aceite: texto "HOLD" e a peça visíveis, sem cobrir outros elementos, tanto no modo de estatísticas `NES` quanto no `TREY V`.

---

### B1 — T-spin, back-to-back, combo e perfect clear ✅ (lógica) / ⚠️ (tela)
**Objetivo:** pontuação guideline completa. **A lógica abaixo foi compilada e coberta por 22 testes headless (todos passando).**

Regras implementadas (× `max(1, level)`):
- Linhas normais: 0/100/300/500/800.
- T-spin completo: 0 linhas 400, 1 linha 800, 2 linhas 1200, 3 linhas 1600.
- T-spin mini: 0 linhas 100, 1 linha 200, 2 linhas 400.
- **Back-to-back:** ×1,5 em clears "difíceis" (Tetris ou qualquer T-spin com linhas) quando o clear difícil anterior também foi difícil (um clear sem dificuldade com linhas quebra a corrente; um lock sem linhas **não** quebra).
- **Combo:** `50 × combo × nível` a partir do 2º clear consecutivo; um lock sem linhas zera.
- **Perfect clear:** +800/1200/1800/2000 (1/2/3/4 linhas) × nível.
- **Detecção T-spin (3 cantos):** peça é T, `srs`, e o **último movimento foi uma rotação**; conta os 4 cantos da caixa 3×3 (parede/chão contam como cheios, acima do campo conta como vazio); ≥3 cantos = T-spin. **Completo** se os 2 cantos da "frente" (lado para o qual o T aponta) estão cheios **ou** o último kick usado foi o de índice 4; senão **mini**.

Arquitetura (importante):
- A classificação (`classifyTSpin`) deve rodar **antes** de a peça ser adicionada ao grid → feita dentro de `lockActiveTetromino`.
- `scoreModernLock(state, lines)` roda no processador logo após `checkForLineClears`. Com 0 linhas soma o bônus na hora; com linhas guarda em `state.pending_clear_score`, e `updateScoreAndLevel` (frame 4 da animação) soma esse valor.
- A flag `last_move_rotation` é ligada por rotação bem-sucedida e desligada por: movimento lateral bem-sucedido, queda (gravidade/soft drop) e hard drop que realmente desceu.

#### B1.1 `include/game_states.hpp`
Acima do `template`, dentro do namespace:
```cpp
enum class TSpin { None, Mini, Full };
```
No construtor, estenda o fim da lista de inicialização (troque `force_lock{} {}`):
```cpp
        force_lock{},
        last_move_rotation{},
        last_kick_index{},
        pending_tspin{TSpin::None},
        combo{-1},
        back_to_back{},
        pending_clear_score{},
        clear_message{},
        clear_message_frames{} {}
```
E os membros, depois de `bool force_lock;`:
```cpp
  // T-spin / back-to-back / combo bookkeeping (modern mode)
  bool last_move_rotation;      // last successful move of the active piece was a rotation
  int last_kick_index;          // SRS kick used by that rotation (0 = none, 4 = last kick)
  TSpin pending_tspin;          // classified at lock time
  int combo;                    // -1 = no combo running
  bool back_to_back;
  int pending_clear_score;      // points for the line clear being animated (added at frame 4)
  std::string clear_message;    // e.g. "B2B T-SPIN DOUBLE"
  int clear_message_frames;
```

#### B1.2 `include/game_logic.hpp`
Substitua a declaração de `rotateTetrominoSrs` e acrescente as duas novas:
```cpp
bool rotateTetrominoSrs(const GameState<>::Grid &grid, const int rotation_offset,
                        TetrominoState &tetromino, int *used_kick = nullptr);
TSpin classifyTSpin(const GameState<> &state);
void scoreModernLock(GameState<> &state, const int lines_cleared);
```

#### B1.3 `src/game_logic.cpp`
(a) Em `rotateTetrominoSrs`: nova assinatura (`..., TetrominoState &tetromino, int *used_kick)`) e troque o laço final por:
```cpp
  int kick_number = 0;
  for (const auto &kick : table[idx]) {
    if (updateStateOnNoCollision(grid, kick.x, -kick.y, rotation_offset, tetromino)) {
      if (used_kick != nullptr) {
        *used_kick = kick_number;
      }
      return true;
    }
    ++kick_number;
  }
  return false;
}
```
(b) Logo depois dessa função, adicione:
```cpp
// 3-corner T-spin detection. Must be called BEFORE the piece is added to the grid.
// T reference point (x, y) is the centre of its 3x3 box; rotation 0 = pointing up, 1 = right,
// 2 = down, 3 = left (SRS states).
TSpin classifyTSpin(const GameState<> &state) {
  const auto &t = state.active_tetromino;
  if (t.tetromino != Tetromino::T || not t.srs || not state.last_move_rotation) {
    return TSpin::None;
  }
  auto occupied = [&state](const int x, const int y) {
    if (x < 0 || x >= static_cast<int>(state.grid.size()) ||
        y >= static_cast<int>(state.grid[0].size())) {
      return true;  // walls and floor count as filled
    }
    if (y < 0) {
      return false;  // above the field counts as empty
    }
    return state.grid[x][y] != 0;
  };
  const bool tl = occupied(t.x - 1, t.y - 1);
  const bool tr = occupied(t.x + 1, t.y - 1);
  const bool bl = occupied(t.x - 1, t.y + 1);
  const bool br = occupied(t.x + 1, t.y + 1);
  if (int{tl} + int{tr} + int{bl} + int{br} < 3) {
    return TSpin::None;
  }
  bool front_corners_filled = false;  // the two corners on the side the T points to
  switch (t.rotation) {
    case 0: front_corners_filled = tl && tr; break;
    case 1: front_corners_filled = tr && br; break;
    case 2: front_corners_filled = bl && br; break;
    default: front_corners_filled = tl && bl; break;
  }
  // The last kick ((+-1, +-2), e.g. the T-spin triple kick) upgrades a mini to a full T-spin.
  return (front_corners_filled || state.last_kick_index == 4) ? TSpin::Full : TSpin::Mini;
}

// Guideline scoring for one lock. Call after the piece was locked and the cleared lines are known.
// 0 lines: the T-spin bonus (if any) is added immediately. >0 lines: the points are stored in
// pending_clear_score and added by updateScoreAndLevel() when the clear animation ends.
void scoreModernLock(GameState<> &state, const int lines_cleared) {
  static const int normal[5] = {0, 100, 300, 500, 800};
  static const int tspin_full[5] = {400, 800, 1200, 1600, 0};
  static const int tspin_mini[5] = {100, 200, 400, 0, 0};
  static const int perfect_clear[5] = {0, 800, 1200, 1800, 2000};
  static const char *names[5] = {"", "SINGLE", "DOUBLE", "TRIPLE", "TETRIS"};

  const int level = std::max(1, state.level);
  const TSpin tspin = state.pending_tspin;
  state.pending_tspin = TSpin::None;

  int base = normal[lines_cleared];
  if (tspin == TSpin::Full) {
    base = tspin_full[lines_cleared];
  } else if (tspin == TSpin::Mini) {
    base = tspin_mini[lines_cleared];
  }
  int points = base * level;

  const bool difficult = lines_cleared > 0 && (lines_cleared == 4 || tspin != TSpin::None);
  const bool b2b_applied = difficult && state.back_to_back;
  if (b2b_applied) {
    points = points * 3 / 2;
  }

  bool perfect = false;
  if (lines_cleared > 0) {
    ++state.combo;
    if (state.combo > 0) {
      points += 50 * state.combo * level;
    }
    state.back_to_back = difficult;
    int filled = 0;
    for (const auto &column : state.grid) {
      for (const int cell : column) {
        filled += cell != 0 ? 1 : 0;
      }
    }
    perfect = (filled - lines_cleared * static_cast<int>(state.grid.size())) == 0;
    if (perfect) {
      points += perfect_clear[lines_cleared] * level;
    }
  } else {
    state.combo = -1;  // a lock without a clear ends the combo (back-to-back is kept)
  }

  if (lines_cleared == 0) {
    state.score += points;
  } else {
    state.pending_clear_score = points;
  }

  std::string message;
  if (b2b_applied) message += "B2B ";
  if (tspin == TSpin::Mini) message += "MINI ";
  if (tspin != TSpin::None) message += "T-SPIN ";
  message += names[lines_cleared];
  if (perfect) message = "PERFECT CLEAR";
  while (!message.empty() && message.back() == ' ') message.pop_back();
  state.clear_message = message;
  state.clear_message_frames = message.empty() ? 0 : 120;
}
```
(c) Em `processKeyEvents` — **três** pequenas edições:

1. Dentro do lambda `move_check_wall_charge`, no ramo de sucesso do movimento:
```cpp
      if (modern) {
        state.last_move_rotation = false;
        registerLockReset(state);
      }
```
2. No bloco de rotação, substitua o cálculo de `rotated` e o `if (rotated)`:
```cpp
    int used_kick = 0;
    const bool rotated =
        modern ? rotateTetrominoSrs(state.grid, rotation, state.active_tetromino, &used_kick)
               : rotateTetromino(state.grid, rotation, wall_kick, state.active_tetromino);
    if (rotated) {
      sample_player.playSample("tetromino_rotate");
      if (modern) {
        state.last_move_rotation = true;
        state.last_kick_index = used_kick;
        registerLockReset(state);
      }
    }
```
3. No bloco do hard drop (`if (modern) { ... }`), acrescente:
```cpp
      if (state.active_tetromino.y != y_before) {
        state.last_move_rotation = false;  // moving down cancels the T-spin condition
      }
```
(d) Em `lockActiveTetromino`, **no início** da função:
```cpp
  if (state.modern_mode) {
    state.pending_tspin = classifyTSpin(state);  // needs the grid WITHOUT the piece
  }
  state.last_move_rotation = false;
  state.last_kick_index = 0;
```
(e) Em `applyModernGravity`, logo após `state.active_tetromino = below;`:
```cpp
    state.last_move_rotation = false;
```
(f) Em `updateScoreAndLevel`, substitua a linha que soma `state.score`:
```cpp
  if (state.modern_mode) {
    state.score += state.pending_clear_score;  // computed by scoreModernLock() at lock time
    state.pending_clear_score = 0;
  } else {
    state.score += getScoreForLineClear(line_clears, state.level);
  }
```

#### B1.4 `src/frame_processors/game_processor.cpp`
Em `doGravityStep`, logo depois de `auto lines_cleared = checkForLineClears(state_);`:
```cpp
    if (state_.modern_mode) {
      scoreModernLock(state_, static_cast<int>(lines_cleared.size()));
    }
```
Em `processFrame`, junto do decremento de `viz_wall_charge_frame_count`:
```cpp
  state_.clear_message_frames = std::max(state_.clear_message_frames - 1, 0);
```

#### B1.5 Mostrar a mensagem na tela ⚠️ (não compilado — confira assinaturas de `drawString`)
`include/game_renderer.hpp` (privado):
```cpp
  void renderClearMessage(const std::string &message,
                          const PixelDrawingInterface::Coords &grid_top_left) const;
```
`src/game_renderer.cpp`:
```cpp
// One word per line, centred over the playfield (80 px wide = 10 chars of 8 px).
void GameRenderer::renderClearMessage(const std::string &message,
                                      const pdi::Coords &grid_top_left) const {
  std::istringstream words(message);
  std::string word;
  int line = 0;
  while (words >> word) {
    const int x = grid_top_left.x + (80 - 8 * static_cast<int>(word.size())) / 2;
    drawer_->drawString(pdi::Coords{x, grid_top_left.y + 56 + line * 10}, word, pdi::WHITE());
    ++line;
  }
}
```
Em `renderGameState`, dentro do `if (state.modern_mode) { ... }`, depois de `renderHold(state);`:
```cpp
    if (state.clear_message_frames > 0) {
      renderClearMessage(state.clear_message, grid_top_left);
    }
```
(A mensagem fica 2 s sobre o campo. O campo é apagado a cada frame, então não deixa resíduo. Palavras máximas: `PERFECT`, `T-SPIN`, `DOUBLE`… todas ≤ 7 caracteres = 56 px.)

#### Critério de aceite B1
- Testes headless (ver C4) com cenários: TSD completo = 1200; segundo TSD seguido = 1800+50 (B2B + combo 1); mini T-spin sem linhas = 100 pts; último kick promove mini→completo; sem rotação final / peça J / 2 cantos = nenhum T-spin; rotação liga a flag e mover desliga; Tetris que esvazia o campo = 800+2000 com mensagem `PERFECT CLEAR`.
- Jogando: um T-spin double real mostra `T-SPIN DOUBLE` e soma 1200×nível; o segundo seguido mostra `B2B T-SPIN DOUBLE`.
- Modo clássico: pontuação idêntica à anterior (`getScoreForLineClear`).

---

### B2 — RNG 7-bag no modo moderno ❓
**Objetivo:** o modo moderno usa 7-bag (guideline) em vez do RNG do NES.

Contexto: `tetromino_rng.cpp` já tem `SevenBagTetrominoRNG`, `NesTetrominoRNG` e `UniformTetrominoRNG`. O tipo vem da opção `rng_type` (`rngTypeFromString(...)` em `menuOptionsToGameOptions`); o `GameProcessor::reset` já recria o RNG com `tetrominoRngFactory(options.rng_type)` **antes** de sortear as primeiras peças.

Passos:
1. Abra `include/tetromino_rng.hpp` e descubra o nome exato do enumerador do 7-bag em `RngType` (ex.: `RngType::SevenBag`).
2. Decisão recomendada: em `menuOptionsToGameOptions` (nestris_x86.cpp), após ler `modern_mode`:
```cpp
  if (options.modern_mode) {
    options.rng_type = RngType::SevenBag;  // use the real enumerator name
  }
```
   Alternativa mais respeitosa: não forçar e apenas documentar que o usuário deve escolher `7-BAG` no menu.
3. Atenção: o construtor de `GameProcessor` usa `tetrominoRngFactory(RngType::Nes)` fixo; isso só vale para o primeiro jogo antes do `reset` — não mude.

Critério de aceite: no modo moderno, sequência de peças respeita 7-bag (cada 7 peças consecutivas, alinhadas à bolsa, contêm as 7 peças).

---

### B3 — Estatísticas (TRT) no modo moderno ❓
**Problema:** `Statistics::update(lines, level)` usa `getScoreForLineClear` (tabela NES) para acumular `score_from_tetrises_`; no modo moderno o TRT (taxa de Tetris) sai inflado. Além disso (bug antigo), `doEntryDelayStep` chama `statistics_.update(..., state_.level)` **depois** de `updateScoreAndLevel`, então no clear que sobe de nível o nível usado já é o novo.

Passos:
1. `include/statistics.hpp`: `void update(const int lines_cleared, const int level, const bool modern = false);`
2. `src/statistics.cpp`:
```cpp
void Statistics::update(const int lines_cleared, const int level, const bool modern) {
  if (lines_cleared == 4) {
    score_from_tetrises_ += modern ? getModernScoreForLineClear(lines_cleared, level)
                                   : getScoreForLineClear(lines_cleared, level);
    burn_counter_ = 0;
  } else {
    burn_counter_ += lines_cleared;
  }
}
```
3. `game_processor.cpp::doEntryDelayStep`, no bloco do frame 4:
```cpp
    const int level_before = state_.level;
    updateScoreAndLevel(static_cast<int>(line_clears_count), *sample_player_, state_);
    statistics_.update(static_cast<int>(line_clears_count), level_before, state_.modern_mode);
```
(`line_clears_count = line_clear_info_.rows.size()`.) Obs.: o TRT moderno ignora o bônus B2B/combo (usa a base 800×nível) — aceitável.

Critério de aceite: no modo clássico o TRT não muda; no moderno um jogo só de Tetris mostra ≈100%.

---

### B4 — High scores separados por modo ⚠️
**Problema:** `state.high_scores` (mapa `int → string`, mostrado como `rbegin()->first`) é compartilhado; pontuações modernas e clássicas se misturam.

Passos (`game_processor.hpp/.cpp`):
1. Membros novos em `GameProcessor`:
```cpp
  std::map<int, std::string> high_scores_classic_{{1000, "HOWARD"}};
  std::map<int, std::string> high_scores_modern_{{1000, "HOWARD"}};
```
(inclua `<map>`/`<string>` se necessário).
2. Em `reset(const GameOptions& options)`, **antes** de recriar o estado:
```cpp
  (state_.modern_mode ? high_scores_modern_ : high_scores_classic_) = state_.high_scores;
```
   e **depois** de `state_ = getNewState(options.level);`:
```cpp
  state_.high_scores = options.modern_mode ? high_scores_modern_ : high_scores_classic_;
```
3. `checkForHighScore` continua igual (insere em `state.high_scores`).

Critério de aceite: fazer 5000 pontos no moderno não altera o high score exibido no clássico e vice-versa.

---

### C1 — Preview de várias próximas peças ⚠️ (opcional)
**Hoje:** só `state.next_tetromino`.
Design sugerido, sem quebrar o clássico:
1. `GameState`: `std::deque<Tetromino> preview_queue;` (peças 2 a 5 da fila).
2. `getNewState`: depois de `state.next_tetromino = getRandomTetromino();`, preencher `preview_queue` com 4 peças sorteadas.
3. `spawnNewTetromino`, no ramo que não vem do Hold, em vez de `state.next_tetromino = getRandomTetromino();`:
```cpp
    if (state.modern_mode && not state.preview_queue.empty()) {
      state.next_tetromino = state.preview_queue.front();
      state.preview_queue.pop_front();
      state.preview_queue.push_back(getRandomTetromino());
    } else {
      state.next_tetromino = getRandomTetromino();
    }
```
   (A ordem de sorteio muda de forma consistente; no clássico nada muda.)
4. Render: desenhar as peças da fila em miniatura (`fillRect` 4×4 px por bloco) em coluna, perto da caixa NEXT (x≈191, y≈135+). **O espaço livre à direita é pequeno** (a tela é 256×225; veja DAS meter em y=175 e controle em y=196): ajuste olhando o jogo rodando.
5. Cuidado: `statistics_.update(Tetromino)` conta peças ao spawnar — não conte peças da fila.

---

### C2 — IHS/IRS (opcional) ⚠️
Hoje nenhuma tecla é processada durante a animação de linha. Para "Initial Hold System": em `doEntryDelayStep`, se `modern_mode` e `Hold.pressed`, grave `state_.ihs_buffered = true` (novo campo) e, no primeiro `doGravityStep` após o spawn, se `ihs_buffered && not hold_used`, chame `holdTetromino` e limpe a flag. IRS análogo com `RotateClockwise/AntiClockwise` ao spawnar. Zere os buffers em `getNewState`.

---

### C3 — Extras (opcionais) ❓
- **Toggle do fantasma:** nova opção booleana `ghost_piece` (mesmo padrão da A2), campo em `GameOptions`/`GameState` (`show_ghost`), e `renderGhost` só se ligado.
- **Som próprio do Hold:** hoje reusa `"tetromino_rotate"`. Para um som novo, adicione `assets/sounds/hold.wav` e regenere `src/assets_cpp/sounds.cpp` com o alvo `asset_cpp_gen` (leia `src/tools/asset_cpp_gen.cpp` para o modo de uso), depois `sample_player.playSample("hold")`.
- **Hold no desenho do controle:** `renderControls` só desenha direções/A/B/Start; não mostra o Hold (sprite `"controller"` é fixo). Se quiser, desenhe um quadradinho extra em posição livre do sprite quando `KeyAction::Hold` estiver pressionado.
- **Linha de entrega do modo:** mostrar `MODERN` na tela de jogo.

---

### C4 — Testes automatizados headless ✅ (recomendado)
A lógica de jogo (`game_logic.cpp`) pode ser testada **sem SDL/olc** compilando com *stubs*. 163 verificações foram escritas assim (SRS, kicks, hold, hard drop, lock delay, gravidade, pontuação, animação/sons, fantasma, T-spin/B2B/combo/PC, modo clássico). Use esta estrutura (pasta `tests/`):

```
tests/
 ├─ run_tests.sh
 ├─ test_modern_mechanics.cpp
 └─ stubs/{das.hpp, gravity.hpp, sound.hpp, statistics.hpp,
           input_devices/input_interface.hpp, utils/logging.hpp}
```

**Stubs mínimos** (substituem headers que puxam SDL/olc):
```cpp
// stubs/das.hpp
#pragma once
namespace nestris_x86 {
class Das {
 public:
  static constexpr int NTSC_FULL_CHARGE = 16;
  static constexpr int NTSC_MIN_CHARGE = 10;
  Das(int full = NTSC_FULL_CHARGE, int min = NTSC_MIN_CHARGE) : full_(full), min_(min) {}
  bool dasFullyCharged(int c) const { return c >= full_; }
  bool dasSoftlyCharged(int c) const { return c >= min_; }
  void softResetDas(int &c) const { c = min_; }
  void hardResetDas(int &c) const { c = 0; }
  void fullyChargeDas(int &c) const { c = full_; }
 private:
  int full_, min_;
};
}
// stubs/gravity.hpp
#pragma once
namespace nestris_x86 { class Gravity { public: int getGravity(int) const { return 48; } }; }
// stubs/sound.hpp  (conta quantas vezes cada som tocou)
#pragma once
#include <map>
#include <string>
namespace sound {
class SoundPlayer {
 public:
  bool playSample(const std::string &n) const { ++counts[n]; return true; }
  mutable std::map<std::string, int> counts;
};
}
// stubs/statistics.hpp  -> arquivo vazio com "#pragma once"
// stubs/input_devices/input_interface.hpp
#pragma once
#include <string>
class InputInterface { public: using KeyCode = int;
  virtual KeyCode lookupKeyCode(const std::string&) const { return 0; } virtual ~InputInterface() = default; };
// stubs/utils/logging.hpp
#pragma once
#include <iostream>
#define LOG_ERROR(x) (std::cerr << x << "\n")
#define LOG_INFO(x) (std::cerr << x << "\n")
```
(Ajuste os nomes/valores do stub `Das` se o `das.hpp` real tiver outra API — leia o real.)

**`run_tests.sh`** — copia só a lógica real para uma pasta temporária (para os stubs terem prioridade sobre os headers reais, pois `#include "x.hpp"` procura primeiro no diretório do arquivo que inclui):
```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT=..
B=$(mktemp -d)
for f in game_logic game_states key_defines tetromino; do cp $ROOT/include/$f.hpp "$B"/; done
cp $ROOT/src/game_logic.cpp "$B"/
g++ -std=c++17 -Wno-sign-compare -Wno-unused -Wno-reorder -Istubs -I"$B" \
    test_modern_mechanics.cpp "$B/game_logic.cpp" -o "$B/test_modern"
"$B/test_modern"
```

**Estrutura do teste** (`test_modern_mechanics.cpp`): um macro `CHECK(cond, msg)` que conta falhas, e uma struct `Sim` que **espelha** `GameProcessor::doGravityStep/doEntryDelayStep` de forma headless: fila de peças roteirizada (`std::deque<Tetromino>`), `spawn()` igual a `spawnNewTetromino` (usa `forced_spawn_tetromino`), e `frame(KeyEvents)` que faz: entrada de atraso → `animateLineClear` + `updateScoreAndLevel` no frame 4; senão `spawn` → `processKeyEvents` → `spawn` → `applyGravity` → (se travou) `press_down_lock`, `addPressDownScore`, `checkForLineClears`, `scoreModernLock` (modo moderno), `updateEntryDelayForLineClear(..., modern)`. Helpers: `press(KeyAction)`, `holdKey(KeyAction)`, `none()`.
Casos a cobrir (cada um com expectativa numérica):
1. Todas as 7 peças × 4 estados SRS têm 4 blocos; spawn moderno fica nas linhas 0–1 e não colide.
2. 4× CW e 4× CCW voltam ao estado inicial; I vertical colada na parede esquerda gira com kick `(+2,0)`; Z e I no chão giram (floor kick).
3. Hold: 1º puxa a próxima; segundo no mesmo turno é ignorado; após travar libera; troca não consome a fila; peça do hold nasce em SRS rotação 0, y=1.
4. Hard drop: `pontos = 2 × células`; trava no mesmo frame; I horizontal acaba na linha 19; `gravity_counter > 1` após travar; próxima peça nasce em y=1.
5. Lock delay ≈ 30 frames; mover reseta (máx. 15); girar o O não toca som nem reseta.
6. `modernGravityFrames`: nível 0/1 = 60, decrescente, nível 30 = 1; soft drop 1 pt/célula e pontos não somem ao soltar Down; Tetris (4 linhas) toca som `tetris`, soma ≥ 800, limpa o campo; 1 linha toca `line_clear`; `entry_delay_counter ≥ 24` no moderno.
7. Fantasma não altera a peça e pousa onde a peça pousaria.
8. T-spin/B2B/combo/PC (seção B1).
9. Modo clássico: spawn NES, Hold ignorado, Up sem hard drop não faz nada.

Critério de aceite: `bash tests/run_tests.sh` imprime `N/N verificações OK`.

---

## 5. Armadilhas conhecidas (leia!)
1. **Declaração ≠ definição:** a maioria dos erros do usuário foi função implementada no `.cpp` e esquecida no `.hpp` (ou assinatura diferente). Sempre atualize os dois.
2. **`GameState` é template com lista de inicialização** — membro novo sem entrada na lista fica não inicializado em certos contextos.
3. **`getKeyEvents()` usa `.at(action)` nos dois mapas de bindings** — falta de qualquer ação lança exceção em tempo de execução, não de compilação.
4. **Pontuação de linhas ocorre no frame 4 da animação**, não no lock. Bônus sem linhas devem ser somados no lock.
5. **`classifyTSpin` precisa rodar antes de `addTetrominoToGrid`** (os cantos do T não fazem parte da peça, mas outras peças/linhas travadas sim; e depois do lock a peça já está no grid).
6. **`state.active_tetromino.y = -10`** é a convenção para "peça estacionada fora do grid" após travar/hold; qualquer código que use a peça ativa deve respeitar `spawn_new_tetromino`/`entryDelay`.
7. **O quadrado (Square) tem offset `{1,0}`** e spawna em `y=0`; I tem caixa 4×4 com offset `{2,2}`. Não generalize "centro = 1,1".
8. **`rand()` em `updateEntryDelayForLineClear`** é intencional (emula o NES) — não substitua por RNG determinístico sem necessidade.
9. **Não confunda `press_down_counter` (clássico, pontos só se Down ficar apertado até travar) com o soft drop moderno** (pontos somados direto em `state.score`).
10. **Mapeamento de enumeradores:** `Tetromino{int}`/`static_cast<Tetromino>(i)` dependem da ordem do enum; não reordene.
11. **Resolução fixa 256×225:** `OnUserCreate` recusa outra resolução; todo layout novo deve caber nisso e em fonte de 8 px.
12. **Fora de escopo/limitações conhecidas:** o campo tem só 20 linhas (sem zona oculta); "lock out" não é tratado separadamente — o top-out ocorre quando a peça nasce colidindo (`spawnNewTetromino` retorna `false`).

---

## 6. Checklist final de aceitação (manual)
1. `make` sem erros nem warnings novos relevantes.
2. Menu → ativar **MODERN** → iniciar. Aparecem: caixa HOLD, fantasma cinza, peças nascendo nas 2 primeiras linhas.
3. **Hold:** `C` guarda a peça; segundo `C` na mesma peça é ignorado; após travar volta a funcionar; a peça trocada reaparece no topo sem pular linha.
4. **Hard drop:** `↑` trava na hora onde o fantasma estava; soma 2 pts/célula.
5. **Soft drop:** `↓` desce 1 linha/frame e soma 1 pt/célula; segurar `↓` continua na peça seguinte.
6. **Rotação SRS:** girar contra paredes e no chão usa kicks; T-spin real é detectado (mensagem na tela).
7. **Lock delay:** peça no chão aguarda ~0,5 s e reseta ao mover/girar (até 15×).
8. **Som** de line clear/tetris toca no moderno; animação de linha normal.
9. **Persistência:** `config.yaml` guarda `modern_mode`; config antigo carrega.
10. **Clássico intacto:** com MODERN desligado o jogo é idêntico ao original (sem Hold, sem fantasma, spawn NES, pontuação NES).
11. (Se feito) high scores separados, 7-bag no moderno, TRT correto, `bash tests/run_tests.sh` verde.
