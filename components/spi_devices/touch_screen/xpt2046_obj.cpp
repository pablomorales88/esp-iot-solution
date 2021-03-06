// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "iot_xpt2046.h"
#include "iot_xpt_priv.h"
#include "driver/gpio.h"

CXpt2046::CXpt2046(xpt_conf_t * xpt_conf, int w, int h, float xfactor, float yfactor, int xoffset, int yoffset)
{
    iot_xpt2046_init(xpt_conf, &m_spi);
    m_pressed = false;
    m_rotation = 0;
    m_io_irq = xpt_conf->pin_num_irq;
    m_offset_x = xoffset;
    m_offset_y = yoffset;
    m_width = w;
    m_height = h;
    m_xfactor = xfactor;
    m_yfactor = yfactor;
}

bool CXpt2046::is_pressed()
{
    return m_pressed;
}

int CXpt2046::get_irq()
{
    return gpio_get_level((gpio_num_t) m_io_irq);
}

int CXpt2046::get_sample(uint8_t command)
{
    return iot_xpt2046_readdata(m_spi, command, 1);
}

void CXpt2046::sample()
{
    position samples[XPT2046_SMPSIZE];
    position distances[XPT2046_SMPSIZE];
    m_pressed = true;

    int aveX = 0;
    int aveY = 0;

    for (int i = 0; i < XPT2046_SMPSIZE; i++) {
        samples[i].x = get_sample(TOUCH_CMD_X);
        samples[i].y = get_sample(TOUCH_CMD_Y);

        if (samples[i].x == 0 || samples[i].x == 4095 || samples[i].y == 0
                || samples[i].y == 4095) {
            m_pressed = false;
        }
        aveX += samples[i].x;
        aveY += samples[i].y;
    }

    aveX /= XPT2046_SMPSIZE;
    aveY /= XPT2046_SMPSIZE;
    for (int i = 0; i < XPT2046_SMPSIZE; i++) {
        distances[i].x = i;
        distances[i].y = ((aveX - samples[i].x) * (aveX - samples[i].x))
                         + ((aveY - samples[i].y) * (aveY - samples[i].y));
    }

    // sort by distance
    for (int i = 0; i < XPT2046_SMPSIZE - 1; i++) {
        for (int j = 0; j < XPT2046_SMPSIZE - 1; j++) {
            if (samples[j].y > samples[j + 1].y) {
                int yy = samples[j + 1].y;
                samples[j + 1].y = samples[j].y;
                samples[j].y = yy;
                int xx = samples[j + 1].x;
                samples[j + 1].x = samples[j].x;
                samples[j].x = xx;
            }
        }
    }

    int tx = 0;
    int ty = 0;
    for (int i = 0; i < (XPT2046_SMPSIZE / 2); i++) {
        tx += samples[distances[i].x].x;
        ty += samples[distances[i].x].y;
    }

    m_pos.x = tx / (XPT2046_SMPSIZE / 2);
    m_pos.y = ty / (XPT2046_SMPSIZE / 2);
}

void CXpt2046::set_rotation(int r)
{
    m_rotation = r % 4;
}

position CXpt2046::getposition()
{
    return m_pos;
}

int CXpt2046::x()
{
    int x = m_offset_x + m_pos.x * m_xfactor;
    int y = m_offset_y + m_pos.y * m_yfactor;

    if(x > m_height){
        x = m_height;
    } else if(x < 0){
        x = 0;
    }
    if(y > m_width){
        y = m_width;
    } else if(y < 0){
        y = 0;
    }
    switch (m_rotation) {
        case 0:
            return x;
        case 3:
            return m_width - y;
        case 2:
            return m_height - x;
        case 1:
            return y;
    }
    return 0;
}

int CXpt2046::y()
{
    int x = m_offset_x + m_pos.x * m_xfactor;
    int y = m_offset_y + m_pos.y * m_yfactor;

    if(x > m_height){
        x = m_height;
    } else if(x < 0){
        x = 0;
    }
    if(y > m_width){
        y = m_width;
    } else if(y < 0){
        y = 0;
    }
    switch (m_rotation) {
        case 0:
            return y;
        case 3:
            return x;
        case 2:
            return m_width - y;
        case 1:
            return m_height - x;
    }
    return 0;
}

void CXpt2046::calibration()
{
    uint16_t px[2], py[2], xPot[4], yPot[4];

    //left-top
    do {
        sample();
    } while (!is_pressed());

    xPot[0] = getposition().x;
    yPot[0] = getposition().y;

    //right-top
    do {
        sample();
    } while (!is_pressed());

    xPot[1] = getposition().x;
    yPot[1] = getposition().y;

    //right-bottom
    do {
        sample();
    } while (!is_pressed());

    xPot[2] = getposition().x;
    yPot[2] = getposition().y;

    //left-bottom
    do {
        sample();
    } while (!is_pressed());
    xPot[3] = getposition().x;
    yPot[3] = getposition().y;


    px[0] = (xPot[0] + xPot[1]) / 2;
    py[0] = (yPot[0] + yPot[3]) / 2;

    px[1] = (xPot[2] + yPot[3]) / 2;
    py[1] = (yPot[2] + yPot[1]) / 2;


    m_xfactor = (float) m_height / (px[1] - px[0]);
    m_yfactor = (float) m_width / (py[1] - py[0]);

    m_offset_x = (int16_t) m_height - ((float) px[1] * m_xfactor);
    m_offset_y = (int16_t) m_width - ((float) py[1] * m_yfactor);
}

void CXpt2046::set_offset(float xfactor, float yfactor, int x_offset, int y_offset)
{
    m_xfactor = xfactor;
    m_yfactor = yfactor;
    m_offset_x = x_offset;
    m_offset_y = y_offset;
}

