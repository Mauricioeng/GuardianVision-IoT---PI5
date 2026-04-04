import streamlit as st
import pandas as pd
import numpy as np
import requests
import plotly.express as px
import plotly.graph_objects as go
from sklearn.ensemble import IsolationForest
import streamlit.components.v1 as components

# --- INICIALIZAÇÃO DE ESTADO (Para o robô saber quando falar) ---
if "falando" not in st.session_state:
    st.session_state.falando = False
if "mensagens" not in st.session_state:
    st.session_state.mensagens = [{"role": "assistant", "content": "Olá! Estou monitorando o clima e a qualidade do ar. Como posso ajudar?"}]

# --- CONFIGURAÇÃO DA PÁGINA ---
st.set_page_config(page_title="Guardian IA | Radar Cidadão", page_icon="🌍", layout="wide")

st.markdown("""
    <style>
    .metric-card { background-color: #1e293b; padding: 20px; border-radius: 12px; border-left: 5px solid #10b981; color: white; margin-bottom: 15px;}
    .alert-card { background-color: #7f1d1d; padding: 20px; border-radius: 12px; border-left: 5px solid #ef4444; color: white; animation: pulse 2s infinite; }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.8; } 100% { opacity: 1; } }
    </style>
""", unsafe_allow_html=True)

# --- INTEGRAÇÃO COM THINGSPEAK ---
MEU_CANAL = '3282069'
API_KEY = 'IK2EOICZPD2MVY7U'

@st.cache_data(ttl=30)
def buscar_dados_iot():
    url = f"https://api.thingspeak.com/channels/{MEU_CANAL}/feeds.json?api_key={API_KEY}&results=100"
    try:
        response = requests.get(url)
        dados = response.json()
        if 'feeds' not in dados or len(dados['feeds']) == 0:
            return pd.DataFrame()
        
        df = pd.DataFrame(dados['feeds'])
        df['created_at'] = pd.to_datetime(df['created_at'])
        df.rename(columns={'field1': 'Temperatura', 'field2': 'Umidade', 'field3': 'Gas_MQ135', 'field4': 'Pressao'}, inplace=True)
        
        for col in ['Temperatura', 'Umidade', 'Gas_MQ135', 'Pressao']:
            df[col] = pd.to_numeric(df[col], errors='coerce').interpolate()
            
        return df
    except Exception as e:
        return pd.DataFrame()

# --- COMPONENTE DO ROBÔ ANIMADO (HTML/REACT EMBUTIDO) ---
def renderizar_guardian(emocao="normal"):
    html_code = f"""
    <!DOCTYPE html>
    <html>
    <head>
        <script src="https://cdn.tailwindcss.com"></script>
        <style>
            .scanlines {{ background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%), linear-gradient(90deg, rgba(255, 0, 0, 0.06), rgba(0, 255, 0, 0.02), rgba(0, 0, 255, 0.06)); background-size: 100% 4px, 3px 100%; }}
            @keyframes float {{ 0%, 100% {{ transform: translateY(0); }} 50% {{ transform: translateY(-8px); }} }}
            @keyframes blink-lid {{ 0%, 96%, 98%, 100% {{ height: 12px; }} 97%, 99% {{ height: 100%; }} }}
            @keyframes talk {{ 0%, 100% {{ height: 8px; border-radius: 10px; }} 50% {{ height: 22px; border-radius: 20px; }} }}
            
            .animate-float {{ animation: float 4s ease-in-out infinite; }}
            .animate-blink-lid {{ animation: blink-lid 5s infinite; }}
            .animate-talk {{ animation: talk 0.2s infinite; }}
        </style>
    </head>
    <body class="bg-transparent flex justify-center items-center m-0 p-0 overflow-hidden">
        <div class="relative flex justify-center items-end h-64 w-full max-w-[300px] mx-auto pb-4 z-10 pt-8">
            <div class="flex flex-col items-center">
                <div class="w-1.5 h-6 bg-gray-400 relative rounded-t-sm z-0">
                    <div class="absolute -top-3 -left-1.5 w-4 h-4 rounded-full shadow-[0_0_15px_currentColor] {"bg-red-500 text-red-500 animate-ping" if emocao == 'alert' else "bg-emerald-400 text-emerald-400 animate-pulse"}"></div>
                </div>
                <div class="bg-[#0a0a0a] rounded-3xl flex flex-col items-center justify-center border-[6px] {"border-red-600 shadow-[0_0_30px_rgba(255,0,0,0.6)]" if emocao == 'alert' else "border-gray-600 shadow-[0_0_20px_rgba(0,128,128,0.5)]"} relative overflow-hidden transition-all duration-500 animate-float w-56 h-48 z-10">
                    <div class="absolute inset-0 scanlines opacity-30 pointer-events-none"></div>
                    <div class="absolute inset-0 shadow-[inset_0_0_20px_rgba(0,0,0,0.8)] pointer-events-none"></div>
                    
                    <div class="flex gap-6 mb-4 z-20 mt-4">
                        <div class="bg-white rounded-full relative overflow-hidden flex justify-center w-12 h-16 shadow-[0_0_20px_rgba(255,255,255,0.8)] {"bg-red-100" if emocao == 'alert' else ""}">
                            <div class="absolute top-0 w-full {"bg-red-600" if emocao == 'alert' else "bg-emerald-400"} z-10 animate-blink-lid" style="height: 16px;"></div>
                            <div class="bg-black rounded-full absolute transition-all duration-300 w-5 h-5 bottom-1 right-1"></div>
                        </div>
                        <div class="bg-white rounded-full relative overflow-hidden flex justify-center w-12 h-16 shadow-[0_0_20px_rgba(255,255,255,0.8)] {"bg-red-100" if emocao == 'alert' else ""}">
                            <div class="absolute top-0 w-full {"bg-red-600" if emocao == 'alert' else "bg-emerald-400"} z-10 animate-blink-lid" style="height: 16px;"></div>
                            <div class="bg-black rounded-full absolute transition-all duration-300 w-5 h-5 bottom-1 right-1"></div>
                        </div>
                    </div>
                    
                    <!-- BOCA DO ROBÔ (Onde a mágica da fala acontece) -->
                    <div id="mouth" class="z-20 w-16 h-2 {"bg-red-300 rounded-full" if emocao == 'alert' else "bg-white rounded-b-full"} transition-all duration-300" style="{"height: 15px;" if emocao == 'alert' else ""}"></div>
                </div>
                <div class="bg-gray-700 border-x-4 border-b-4 border-gray-900 rounded-b-xl shadow-inner z-0 w-16 h-6"></div>
            </div>
        </div>

        <script>
            // Lógica Javascript: Se o Python disser que está falando, ativa a animação e desliga depois de 3.5 segundos
            window.onload = () => {{
                const mouth = document.getElementById('mouth');
                if ("{emocao}" === "falando") {{
                    mouth.classList.add('animate-talk');
                    setTimeout(() => {{
                        mouth.classList.remove('animate-talk');
                    }}, 3500); 
                }}
            }}
        </script>
    </body>
    </html>
    """
    components.html(html_code, height=300)

# --- LÓGICA PRINCIPAL ---
df = buscar_dados_iot()

if df.empty:
    st.error("Conectando aos sensores IoT... Aguardando transmissão do ThingSpeak.")
else:
    ultimo = df.iloc[-1]
    
    # 1. PROCESSAMENTO MATEMÁTICO E ML
    df['Derivada_Pressao'] = df['Pressao'].diff() 
    tendencia_pressao = df['Derivada_Pressao'].rolling(window=5).mean().iloc[-1]
    
    prob_chuva = 10
    if tendencia_pressao < -0.5: prob_chuva = 85
    elif tendencia_pressao < -0.1: prob_chuva = 50
    
    X = df[['Temperatura', 'Umidade', 'Gas_MQ135']].dropna()
    modelo_ia = IsolationForest(contamination=0.05, random_state=42)
    df.loc[X.index, 'Anomalia'] = modelo_ia.fit_predict(X)
    anomalia_atual = df['Anomalia'].iloc[-1] == -1

    # Define qual será o comportamento visual do robô nesta rodada
    if anomalia_atual:
        estado_robo = "alert"
    elif st.session_state.falando:
        estado_robo = "falando"
    else:
        estado_robo = "normal"
        
    # Reseta o estado de fala para que ele não fale para sempre
    st.session_state.falando = False 

    # 2. CABEÇALHO COM O ROBÔ ANIMADO
    c1, c2 = st.columns([1, 2])
    with c1:
        renderizar_guardian(emocao=estado_robo)
    with c2:
        st.title("Guardian IA | Radar Cidadão")
        st.markdown("**Motor Python Avançado:** Conectado à rede neural Edge e sensores ambientais ODS 3.")
        
        if anomalia_atual:
            st.markdown("""
            <div class="alert-card">
                <h3 style="margin:0;">🚨 ANOMALIA AMBIENTAL DETECTADA!</h3>
                <p style="margin:0;">O Guardião identificou um padrão multivariado crítico (Risco respiratório ou incêndio).</p>
            </div>
            """, unsafe_allow_html=True)
        else:
            st.success("✅ Ambiente Estável. ODS 3 dentro dos padrões aceitáveis.")

    st.markdown("---")

    # 3. PAINEL DE MÉTRICAS NATIVO
    col1, col2, col3, col4 = st.columns(4)
    
    col1.markdown(f"""
    <div class="metric-card">
        <h4 style="margin:0; color:#cbd5e1;">Termômetro DHT22</h4>
        <h2 style="margin:0; color:white;">{ultimo['Temperatura']:.1f} °C</h2>
        <small>Z-Score: {((ultimo['Temperatura'] - df['Temperatura'].mean()) / df['Temperatura'].std()):.2f}σ</small>
    </div>
    """, unsafe_allow_html=True)
    
    col2.markdown(f"""
    <div class="metric-card" style="border-left-color: #3b82f6;">
        <h4 style="margin:0; color:#cbd5e1;">Umidade Relativa</h4>
        <h2 style="margin:0; color:white;">{ultimo['Umidade']:.0f} %</h2>
        <small>Média 24h: {df['Umidade'].mean():.1f}%</small>
    </div>
    """, unsafe_allow_html=True)
    
    col3.markdown(f"""
    <div class="metric-card" style="border-left-color: {'#ef4444' if ultimo['Gas_MQ135'] > 800 else '#eab308'};">
        <h4 style="margin:0; color:#cbd5e1;">Qualidade do Ar</h4>
        <h2 style="margin:0; color:white;">{ultimo['Gas_MQ135']:.0f} ppm</h2>
        <small>{'ALERTA ODS 3: Insalubre' if ultimo['Gas_MQ135'] > 800 else 'Ar Saudável'}</small>
    </div>
    """, unsafe_allow_html=True)
    
    col4.markdown(f"""
    <div class="metric-card" style="border-left-color: #8b5cf6;">
        <h4 style="margin:0; color:#cbd5e1;">Pressão</h4>
        <h2 style="margin:0; color:white;">{ultimo['Pressao']:.1f} hPa</h2>
        <small>Velocidade (ΔP): {tendencia_pressao:.2f} hPa/ciclo</small>
    </div>
    """, unsafe_allow_html=True)

    # 4. GRÁFICOS E CHAT (PLOTLY)
    aba1, aba2, aba3 = st.tabs(["Chat IA ODS 3", "Série Temporal", "Machine Learning"])
    
    with aba1:
        st.subheader("🤖 Conversar com o Guardian")
        st.write("Me pergunte sobre os sensores ou converse sobre a ODS 3.")
        
        # Histórico de mensagens
        for msg in st.session_state.mensagens:
            with st.chat_message(msg["role"]):
                st.markdown(msg["content"])
                
        # Captura de input do usuário
        if prompt := st.chat_input("Pergunte ao Guardian..."):
            # 1. Adiciona e exibe o que o usuário digitou
            st.session_state.mensagens.append({"role": "user", "content": prompt})
            
            # 2. Formula a resposta inteligente com base nos sensores
            resposta = f"Analisando os sensores no momento (Temperatura: {ultimo['Temperatura']}°C, Gás: {ultimo['Gas_MQ135']}ppm). "
            if "chuva" in prompt.lower() or "chover" in prompt.lower():
                resposta += f"Baseado no cálculo de queda de pressão, temos {prob_chuva}% de chance de precipitação."
            elif "ods" in prompt.lower() or "ar" in prompt.lower() or "saúde" in prompt.lower():
                resposta += "Meu objetivo pela ODS 3 é proteger suas vias respiratórias. O ar " + ("está PERIGOSO no momento!" if ultimo['Gas_MQ135'] > 800 else "está limpo e respirável.")
            else:
                resposta += "Minha IA cruza essas métricas para manter seu ambiente o mais seguro possível contra anomalias e estresse térmico."
                
            # 3. Adiciona a resposta do bot
            st.session_state.mensagens.append({"role": "assistant", "content": resposta})
            
            # 4. GATILHO MÁGICO: Avisa que o robô precisa falar e recarrega a página instantaneamente!
            st.session_state.falando = True
            st.rerun()

    with aba2:
        fig = go.Figure()
        fig.add_trace(go.Scatter(x=df['created_at'], y=df['Temperatura'], name="Temperatura (°C)", line=dict(color='red', width=2)))
        fig.add_trace(go.Scatter(x=df['created_at'], y=df['Umidade'], name="Umidade (%)", line=dict(color='blue', width=2), yaxis="y2"))
        
        fig.update_layout(
            title="Evolução Térmica e de Umidade",
            yaxis=dict(title="Temperatura (°C)", title_font=dict(color="red"), tickfont=dict(color="red")),
            yaxis2=dict(title="Umidade (%)", title_font=dict(color="blue"), tickfont=dict(color="blue"), anchor="x", overlaying="y", side="right"),
            plot_bgcolor='rgba(0,0,0,0)', paper_bgcolor='rgba(0,0,0,0)', hovermode="x unified"
        )
        st.plotly_chart(fig, use_container_width=True)

    with aba3:
        st.write("A inteligência artificial isola os pontos vermelhos (anomalias) analisando como a Temperatura se cruza com o nível de Gás.")
        df['Status_IA'] = df['Anomalia'].apply(lambda x: 'Anomalia' if x == -1 else 'Normal')
        fig_scatter = px.scatter(df, x='Temperatura', y='Gas_MQ135', color='Status_IA', 
                                 color_discrete_map={'Normal': '#10b981', 'Anomalia': '#ef4444'},
                                 title="Análise de Anomalias (Isolation Forest)")
        st.plotly_chart(fig_scatter, use_container_width=True)