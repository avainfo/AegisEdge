# AegisHorizonEdge

> Detecção de horizonte em tempo real para drones, com fusão IMU + Visão Neural na NPU Hailo-8L

---

## O que é

AegisHorizonEdge é um sistema de detecção de horizonte para drones autónomos que combina dados do IMU (giroscópio + acelerómetro) com inferência por rede neural numa NPU edge (Hailo-8L). O objectivo é atingir 60 fps a 1080p num sistema de baixo consumo (RPi 5 + Hailo-8L) ao **evitar chamar a rede neural em frames consecutivas** quando o IMU sozinho consegue prever o estado com confiança suficiente.

---

## Arquitectura

```
   ┌──────────┐     ┌──────────────┐    ┌──────────────────┐
   │  IMU     │────▶│  Kalman 1-D  │───▶│  Decisão:        │
   │ ~200Hz   │     │  (predict)   │    │  precisa visão?  │
   └──────────┘     └──────────────┘    └────────┬─────────┘
                                                 │
                          ┌──────────────────────┴────────┐
                          │                               │
                          ▼ (sim)                         ▼ (não)
                   ┌─────────────┐                  ┌──────────────┐
                   │ ROI dinâmico│                  │  Dead Reck.  │
                   │ + Hailo NPU │                  │  só por IMU  │
                   └──────┬──────┘                  └──────┬───────┘
                          │                                │
                          ▼                                │
                   ┌─────────────┐                         │
                   │ Kalman      │◀────────────────────────┘
                   │ (update)    │
                   └──────┬──────┘
                          ▼
                  HorizonLine final
```

A ideia central: o IMU corre sempre e propaga a estimativa do horizonte com baixíssimo custo. A visão (Hailo ou Hough) só entra quando há manobras bruscas, refresh periódico, ou na primeira frame. Isto reduz o número de inferências da NPU em ~70-85%, libertando compute para outras tarefas.

---

## Três modos de compilação

O sistema suporta três modos seleccionáveis por flags do compilador. Cada modo gera um binário independente.

| Modo | Flag | Detector de visão | Quando usar |
|---|---|---|---|
| **IMU only** | (nenhum) | Nenhum (só Kalman+IMU) | Baseline mínimo, validar lógica IMU |
| **IMU + Hough** | `-DWITH_HOUGH` | Hough clássico (OpenCV) | Desenvolvimento sem hardware, baseline para tese |
| **IMU + Hailo** | `-DWITH_HAILO` | Rede neural (.hef) | Produção, hardware Hailo-8/8L |

Se ambos `WITH_HAILO` e `WITH_HOUGH` forem passados, `WITH_HAILO` tem prioridade.

No arranque, cada binário imprime qual o modo activo:
```
[Detector] Modo: IMU + Hough (CV classico)
```

---

## Estrutura do projecto

```
AegisEdge/
├── DetectionSystems/
│   ├── horizon.hpp        # Interface, Kalman, modos de compilação
│   └── ohd.cpp            # Pipeline e dispatch de visão
├── drone-sim/
│   ├── main.cpp           # Simulador com dados reais do AirSim
│   └── logs/              # Telemetria CSV
├── Assets/
│   ├── images/            # Frames AirSim (timestamps no nome)
│   ├── logs/              # Telemetria do voo
│   └── Videos/            # Vídeo 1080p alternativo
└── README.md
```

---

## Como funciona

### Os dois caminhos por frame

Em cada frame, o `process()` toma uma decisão:

**Caminho rápido (IMU only)** — `< 0.5ms`:
- Propaga o Kalman com a velocidade angular do giroscópio
- Integra o roll para actualizar o ângulo da linha
- Aplica correcção suave com a estimativa do acelerómetro

**Caminho pesado (visão)** — `~2ms (Hough)` / `~5ms (Hailo)`:
- Recorta um ROI dinâmico onde o Kalman prevê estar o horizonte
- Hailo: pré-processa para 224×224, BGR→RGB, normaliza, corre na NPU
- Hough: Canny + HoughLinesP, escolhe linha mais longa próxima do prior
- Translada coordenadas crop→imagem
- Faz update do Kalman e gating contra falsos positivos

### Triggers para chamar a visão

```cpp
bool needs_vision = !initialized_                              // arranque
                 || (delta_pitch > IMU_THRESHOLD_DEGREES)      // manobra
                 || (delta_roll  > IMU_THRESHOLD_DEGREES)      // manobra
                 || (seconds_since_hailo >= MAX_SECONDS);      // refresh
```

### Filtro de Kalman 1-D

Estado: `[offset_y (px), velocidade_y (px/s)]`

Modelo de propagação: velocidade constante perturbada pela taxa angular do IMU (feedforward).

Ruído de medição:
- `R_imu = 2.0 px²` — confiança média na estimativa do acelerómetro
- `R_hailo = 1.0 px²` — confiança alta na rede neural

A diferença nestes valores faz com que o Kalman se ajuste mais agressivamente quando a visão fala.

### Gating contra falsos positivos

Mesmo que a visão devolva um resultado válido, ele só é aceite se for consistente com a previsão do Kalman:

```cpp
disagreement < (2.0 * kalman_uncertainty + 15px)
```

Isto evita que o Kalman seja "envenenado" por detecções erradas (por exemplo, o Hough a confundir uma cumeeira de montanha com o horizonte real).

### ROI dinâmico

O ROI cresce com a incerteza do Kalman:

```
roi_height = base + (uncertainty_px * scale_factor)
```

Quanto mais tempo sem visão, mais o Kalman duvida da sua posição → ROI maior → menos hipótese do horizonte real cair fora da janela de busca.

---

## Instalação

### Dependências

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libopencv-dev
sudo apt install -y python3 python3-pip python3-venv
```

Verifica que o OpenCV está instalado:

```bash
pkg-config --modversion opencv4
# Deve imprimir 4.x.x
```

### Ambiente Python (para análise/scripts)

```bash
python3 -m venv ~/drone-sim
source ~/drone-sim/bin/activate
pip install opencv-python numpy matplotlib pandas
```

### HailoRT (apenas se tens hardware)

Segue as instruções oficiais da Hailo. Sem o SDK, podes usar os modos `IMU only` ou `IMU + Hough` — não precisas de hardware para desenvolver.

---

## Compilação

A partir da pasta `drone-sim/`:

### Modo IMU only (baseline mínimo)

```bash
g++ main.cpp ../DetectionSystems/ohd.cpp -o sim_imu \
    `pkg-config --cflags --libs opencv4` -std=c++17 -lpthread
```

### Modo IMU + Hough (desenvolvimento)

```bash
g++ main.cpp ../DetectionSystems/ohd.cpp -o sim_hough \
    `pkg-config --cflags --libs opencv4` -std=c++17 -lpthread \
    -DWITH_HOUGH
```

### Modo IMU + Hailo (produção)

```bash
g++ main.cpp ../DetectionSystems/ohd.cpp -o sim_hailo \
    `pkg-config --cflags --libs opencv4` -std=c++17 -lpthread \
    -DWITH_HAILO -lhailort
```

Recomenda-se gerar os três binários para conseguir comparar resultados directamente.

---

## Como correr a simulação

A simulação usa frames reais do AirSim sincronizadas com telemetria do voo (pitch e roll reais).

```bash
cd drone-sim
./sim_hough        # ou ./sim_imu, ./sim_hailo
```

Argumentos opcionais:

```bash
./sim_hough <csv_telemetria> <pasta_frames> <janela_on>
```

Exemplos:
```bash
# Com janela visual
./sim_hough ../Assets/logs/airsim_telemetry_log.csv ../Assets/images/images 1

# Headless (apenas terminal, útil para gerar logs)
./sim_hough ../Assets/logs/airsim_telemetry_log.csv ../Assets/images/images 0
```

### Controlos durante a simulação

- Qualquer tecla — avançar uma frame
- `q` ou `ESC` — sair

---

## Saída esperada

### Visual
A frame é apresentada com uma linha sobreposta:
- **Verde** — horizonte confirmado pela visão (visão chamada e aceite pelo gate)
- **Amarelo** — estimativa apenas por IMU

Em modo IMU-only, a linha é sempre amarela (não há visão).

### Terminal
Cada frame imprime os valores em tempo real:
```
t=1777083619062 | pitch=19.1 | roll=22.6 | offset=174.2 | angle=-22.5 [VISAO]
t=1777083619263 | pitch=14.1 | roll=32.9 | offset=128.4 | angle=-32.7 [IMU]
```

No final, estatísticas globais:
```
========== SIMULACAO CONCLUIDA ==========
Frames processadas: 175
Chamadas a visao  : 49
Frames por IMU    : 126
Poupanca compute  : 72.0%
```

A **poupança computacional** é a métrica chave do projecto.

---

## Comparação directa entre modos

Para a tese, podes correr os três modos em paralelo e comparar:

```bash
./sim_imu   ../Assets/logs/airsim_telemetry_log.csv ../Assets/images/images 0 > log_imu.txt
./sim_hough ../Assets/logs/airsim_telemetry_log.csv ../Assets/images/images 0 > log_hough.txt
./sim_hailo ../Assets/logs/airsim_telemetry_log.csv ../Assets/images/images 0 > log_hailo.txt   # quando tiveres HW
```

Cada log tem o mesmo formato, permitindo análise comparativa em Python (RMSE, latência, taxa de chamadas à visão).

---

## Configuração

Os parâmetros principais estão em `DetectorConfig` (ficheiro `horizon.hpp`):

| Parâmetro | Default | Descrição |
|---|---|---|
| `frame_width / frame_height` | 1920×1080 | Resolução da câmara |
| `v_fov_degrees` | 90.0 | FOV vertical (calibra com a tua câmara!) |
| `imu_threshold_degrees` | 1.5 | Limiar para considerar manobra |
| `roi_height_base` | 256 | Altura base do ROI dinâmico |
| `max_seconds_no_hailo` | 0.5 | Tempo máximo sem chamar visão |
| `roi_uncertainty_scale` | 4 | Quanto o ROI cresce por px de incerteza |

Os parâmetros do Kalman estão em `KalmanHorizon` e devem ser tunados em campo.

---

## Resumo dos modos

| | IMU only | IMU + Hough | IMU + Hailo |
|---|---|---|---|
| Detector de visão | — | Hough clássico (OpenCV) | Rede neural (.hef) |
| Latência da visão | — | ~2ms | ~5ms |
| Robustez | Boa em curto prazo | Média (falha em terreno complexo) | Alta |
| Hardware | Qualquer CPU | Qualquer CPU | Hailo-8/8L |
| Para que serve | Baseline mínimo | Desenvolvimento, baseline para tese | Produção |

A interface da função `callVision()` é **idêntica** nos três modos, por isso o resto da pipeline não muda.

**Nota sobre o Hough:** em cenários com terreno detalhado (montanhas, edifícios, nuvens), o Hough clássico tem dificuldade em distinguir o horizonte verdadeiro de outras linhas horizontais fortes. Esta limitação é precisamente o que justifica o uso de uma rede neural treinada (Hailo) — a rede aprende padrões globais em vez de simplesmente detectar arestas. O modo Hough serve sobretudo como baseline de comparação para a tese.

---

## Próximos passos

### Treino do modelo
- [ ] Recolher dataset com horizontes anotados (AirSim ou voos reais)
- [ ] Treinar regressor leve (MobileNetV3 ou EfficientNet-Lite) com output `[sin(angle), cos(angle), offset_norm]`
- [ ] Exportar para ONNX
- [ ] Compilar para `.hef` com Hailo Dataflow Compiler

### Hardware
- [ ] Testar em Raspberry Pi 5 + Hailo-8L AI Kit
- [ ] Integrar IMU físico (ICM-42688 recomendado)
- [ ] Pipeline GStreamer para câmara CSI a 60 fps

### Avaliação
- [ ] Comparar erro vs ground truth nos três modos
- [ ] Caracterizar trade-off accuracy × FPS
- [ ] Medir consumo energético em voo

---

## Licença



## Referências relevantes

- Ettinger et al. (2003), *Towards Flight Autonomy: Vision-Based Horizon Detection for Micro Air Vehicles*
- Visual-Inertial Odometry (VINS-Mono, MSCKF, OpenVINS)
- HailoRT documentation: https://hailo.ai/developer-zone/
