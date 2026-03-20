import streamlit as st
import pandas as pd
import numpy as np
import time
from datetime import datetime
import plotly.graph_objects as go

st.set_page_config(
    page_title="Eye-Pulse | IoT Glaucoma Monitor",
    page_icon="👁️",
    layout="wide",
    initial_sidebar_state="expanded"
)

st.markdown("""
    <style>
    .main {
        background-color: #f0f2f6;
    }
    .stMetric {
        background-color: #ffffff;
        padding: 15px;
        border-radius: 12px;
        box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    .reportview-container .main .block-container {
        padding-top: 2rem;
    }
    </style>
    """, unsafe_allow_html=True)

if 'iop_history' not in st.session_state:
    st.session_state.iop_history = pd.DataFrame(columns=['Timestamp', 'IOP_mmHg'])

def fetch_esp8266_data():
    base = 16.0
    variation = np.random.normal(0, 0.8)
    drift = np.sin(time.time() / 5) * 4
    return round(base + variation + drift, 2)

with st.sidebar:
    st.title("Eye-Pulse System")
    st.write("---")
    st.subheader("Hardware Status")
    st.success("ESP8266: Online")
    st.info("Sensor: Ultrasonic ARF")
    st.write("---")
    patient_name = st.text_input("Patient Name", "User_Alpha")
    patient_id = st.text_input("Patient ID", "EP-9921")
    st.write("---")
    if st.button("Clear Session"):
        st.session_state.iop_history = pd.DataFrame(columns=['Timestamp', 'IOP_mmHg'])
        st.rerun()

st.title("👁️ Intraocular Pressure Real-Time Dashboard")
st.write(f"Active Session: **{patient_name}** | ID: **{patient_id}**")

placeholder = st.empty()

while True:
    val = fetch_esp8266_data()
    ts = datetime.now().strftime("%H:%M:%S")
    
    entry = pd.DataFrame({'Timestamp': [ts], 'IOP_mmHg': [val]})
    st.session_state.iop_history = pd.concat([st.session_state.iop_history, entry], ignore_index=True)
    
    if len(st.session_state.iop_history) > 60:
        st.session_state.iop_history = st.session_state.iop_history.iloc[1:]

    if val < 10:
        label = "Hypotony"
        color = "off"
        msg = "Pressure too low. Check sensor placement."
    elif 10 <= val <= 21:
        label = "Normal"
        color = "normal"
        msg = "IOP is within safe limits."
    else:
        label = "High Risk"
        color = "inverse"
        msg = "Elevated IOP. Immediate consultation required."

    with placeholder.container():
        c1, c2, c3, c4 = st.columns(4)
        c1.metric("Current Reading", f"{val} mmHg", delta=label, delta_color=color)
        c2.metric("Mean Pressure", f"{round(st.session_state.iop_history['IOP_mmHg'].mean(), 1)} mmHg")
        c3.metric("Peak Value", f"{st.session_state.iop_history['IOP_mmHg'].max()} mmHg")
        c4.metric("Samples", len(st.session_state.iop_history))

        st.write("---")

        g_col, i_col = st.columns([2, 1])

        with g_col:
            st.subheader("Pressure Waveform (ARF Analysis)")
            fig = go.Figure()
            fig.add_trace(go.Scatter(
                x=st.session_state.iop_history['Timestamp'], 
                y=st.session_state.iop_history['IOP_mmHg'],
                mode='lines+markers',
                fill='tozeroy',
                line=dict(color='#007BFF', width=2)
            ))
            fig.add_hrect(y0=10, y1=21, fillcolor="green", opacity=0.1, layer="below", line_width=0)
            fig.update_layout(
                xaxis_title="Timeline",
                yaxis_title="mmHg",
                yaxis=dict(range=[0, 40]),
                margin=dict(l=0, r=0, t=0, b=0),
                height=450,
                template="none"
            )
            st.plotly_chart(fig, use_container_width=True)

        with i_col:
            st.subheader("Instant Gauge")
            gauge = go.Figure(go.Indicator(
                mode = "gauge+number",
                value = val,
                gauge = {
                    'axis': {'range': [None, 40]},
                    'bar': {'color': "#34495e"},
                    'steps' : [
                        {'range': [0, 10], 'color': "#D6EAF8"},
                        {'range': [10, 21], 'color': "#D4EFDF"},
                        {'range': [21, 40], 'color': "#FADBD8"}],
                    'threshold' : {'line': {'color': "red", 'width': 4}, 'value': 21}
                }
            ))
            gauge.update_layout(height=300, margin=dict(l=20, r=20, t=40, b=20))
            st.plotly_chart(gauge, use_container_width=True)

            if val > 21:
                st.error(f"**Action Required:** {msg}")
                st.markdown("""
                **Specialists Nearby:**
                * Eye-Pulse Partner Hospital (2km)
                * Dr. Sarah Chen (Glaucoma Specialist)
                """)
            else:
                st.success(f"**Diagnostic:** {msg}")

        with st.expander("System Metadata"):
            st.table(st.session_state.iop_history.tail(5))
            st.write("Sampling Rate: 1.0 Hz")
            st.write("SDG Targets: 3.d, 9.5, 10.2")

    time.sleep(1)
