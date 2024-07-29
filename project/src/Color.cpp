#include <Color.hpp>

namespace Colors{


    Color::Color(float r, float g, float b, float a)
    {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        a = std::clamp(a, 0.0f, 1.0f);   
    }

    Color::Color() : r(0.0f), g(0.0f), b(0.0f), a(0.0f)
    {
    }

    Color &Color::SetColor(float r, float g, float b, float a)
    {
        this->r = std::clamp(r, 0.0f, 1.0f);
        this->g = std::clamp(g, 0.0f, 1.0f);
        this->b = std::clamp(b, 0.0f, 1.0f);
        this->a = std::clamp(a, 0.0f, 1.0f);

        return *this;   
    }

    Color &Color::SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        return SetColor(float(r)/255.0f, float(g)/255.0f, float(b)/255.0f, float(a)/255.0f);
    }
    unsigned char Color::GetComponentValueAsUByte(ColorComponent component) const
    {
        return static_cast<unsigned char>(GetComponentValueAsFloat(component)) * 255;
    }
    float Color::GetComponentValueAsFloat(ColorComponent component) const
    {
        switch(component)
        {
            case ColorComponent::Red :
                return r;
            case ColorComponent::Green :
                return g;
            case ColorComponent::Blue :
                return b;
            case ColorComponent::Alpha :
                return a;
        }
        return a;
                
        
    }
}