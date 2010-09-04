/*--
    OneParamWidget.cpp  

    This file is part of the Cornucopia curve sketching library.
    Copyright (C) 2010 Ilya Baran (baran37@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "OneParamWidget.h"
#include "ui_OneParamWidget.h"
#include "ParamWidget.h"

using namespace std;
using namespace Eigen;

const int OneParamWidget::_sliderMarks = 100;

OneParamWidget::OneParamWidget(ParamWidget *paramWidget, QWidget *parent, const Cornu::Parameters::Parameter &param)
    : _paramWidget(paramWidget), QWidget(parent), _param(param), _changing(false), _value(param.defaultVal), _infinity(false)
{
    _ui = new Ui::OneParamWidgetUi();
    _ui->setupUi(this);

    _ui->label->setText(_param.typeName.c_str());

    connect(_ui->spinBox, SIGNAL(valueChanged(double)), this, SLOT(setValue(double)));
    connect(this, SIGNAL(spinBoxValueChanged(double)), _ui->spinBox, SLOT(setValue(double)));
    connect(_ui->slider, SIGNAL(valueChanged(int)), this, SLOT(setSliderValue(int)));
    connect(this, SIGNAL(sliderValueChanged(int)), _ui->slider, SLOT(setValue(int)));
    connect(_ui->infinityBox, SIGNAL(toggled(bool)), this, SLOT(setInfinity(bool)));
    connect(this, SIGNAL(infinitySet(bool)), _ui->infinityBox, SLOT(setChecked(bool)));

    connect(paramWidget, SIGNAL(parametersChanged()), this, SLOT(parametersChanged()));

    _ui->slider->setRange(0, _sliderMarks);
    _ui->spinBox->setRange(0, 1e10); //TODO: maximum value slider allows without overflow

    if(!param.infinityAllowed)
    {
        delete _ui->infinityBox;
        _ui->infinityBox = NULL;
    }

    if(param.type > Cornu::Parameters::INTERNAL_PARAMETERS_MARKER)
    {
        delete _ui->slider;
        _ui->slider = NULL;
        QMargins margins = _ui->verticalLayout->contentsMargins();
        margins.setTop(0);
        margins.setBottom(0);
        _ui->verticalLayout->setContentsMargins(margins);
    }

    if(param.defaultVal == Cornu::Parameters::infinity)
        setValue(param.min); //make sure the parameter UI has a default non-infinite value
    setValue(param.defaultVal);

    new ParameterSetter(paramWidget, this);
}

void OneParamWidget::setValue(double value)
{
    if(_changing)
        return;
    _changing = true;

    _infinity = (value == Cornu::Parameters::infinity);
    if(!_infinity)
        _value = value;
    emit valueChanged(value);
    if(_value != _ui->spinBox->value())
        emit spinBoxValueChanged(_value);
    emit sliderValueChanged(_toSlider(_value));
    emit infinitySet(_infinity);

    _changing = false;
}

void OneParamWidget::setInfinity(bool inf)
{
    if(_changing)
        return;
    _infinity = inf;
    setValue(_getValue());
}

void OneParamWidget::setSliderValue(int val)
{
    if(_changing)
        return;
    _value = _fromSlider(val);
    setValue(_getValue());
}

double OneParamWidget::_getValue() const
{
    return _infinity ? Cornu::Parameters::infinity : _value;
}

double OneParamWidget::_fromSlider(int val)
{
    return _param.min + (_param.max - _param.min) * double(val) / _sliderMarks;
}

int OneParamWidget::_toSlider(double val)
{
    return min(_sliderMarks, max(0, (int)floor(0.5 + _sliderMarks * (val - _param.min) / (_param.max - _param.min))));
}

void OneParamWidget::parametersChanged()
{
    double newValue = _paramWidget->parameters().get(_param.type);
    if(newValue != _getValue())
        setValue(newValue);
}

ParameterSetter::ParameterSetter(ParamWidget *paramWidget,  OneParamWidget *oneParamWidget)
    : QObject(oneParamWidget)
{
    _parameter = oneParamWidget->parameter().type;
    connect(oneParamWidget, SIGNAL(valueChanged(double)), this, SLOT(valueChanged(double)));
    connect(this, SIGNAL(setParameter(Cornu::Parameters::ParameterType, double)), paramWidget, SLOT(setParameter(Cornu::Parameters::ParameterType, double)));
}

void ParameterSetter::valueChanged(double value)
{
    emit setParameter(_parameter, value);
}

#include "OneParamWidget.moc"
