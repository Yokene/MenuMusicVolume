#include <Geode/Geode.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <algorithm>

using namespace geode::prelude;

static constexpr auto MENU_MUSIC_KEY = "menu-music-volume";

// ============================================================
// VOLUME
// ============================================================

static float getMenuMusicVolume() {
    return Mod::get()->getSavedValue<float>(
        MENU_MUSIC_KEY,
        1.0f
    );
}

static void setMenuMusicVolume(float value) {
    value = std::clamp(
        value,
        0.0f,
        1.0f
    );

    Mod::get()->setSavedValue<float>(
        MENU_MUSIC_KEY,
        value
    );
}

// ============================================================
// SPRITE HELPER
// ============================================================

static CCSprite* createSliderSprite(const char* name) {

    auto sprite = CCSprite::create(name);

    if (
        sprite &&
        sprite->getContentSize().width > 0
    ) {
        return sprite;
    }

    auto cache =
        CCSpriteFrameCache::sharedSpriteFrameCache();

    if (cache->spriteFrameByName(name)) {
        return CCSprite::createWithSpriteFrameName(
            name
        );
    }

    std::string altName = name;

    size_t dot =
        altName.find(".png");

    if (
        dot != std::string::npos
    ) {

        altName.insert(
            dot,
            "_001"
        );

        if (
            cache->spriteFrameByName(
                altName.c_str()
            )
        ) {
            return CCSprite::createWithSpriteFrameName(
                altName.c_str()
            );
        }
    }

    return nullptr;
}

// ============================================================
// VERTICAL SLIDER
// ============================================================

class VerticalSlider :
    public CCNode,
    public CCTouchDelegate
{
public:

    CCSprite* m_groove = nullptr;
    CCSprite* m_thumb = nullptr;

    // Внешняя маска формы дорожки.
    CCClippingNode* m_grooveClipper = nullptr;

    // Внутренняя маска значения.
    CCClippingNode* m_valueClipper = nullptr;

    // Stencil для высоты заливки.
    CCDrawNode* m_stencil = nullptr;

    float m_value = 1.0f;

    float m_grooveLength = 210.0f;

    float m_edgeMargin = 30.0f;

    bool m_touching = false;

    std::function<void(float)> m_callback;

    // ========================================================
    // CREATE
    // ========================================================

    static VerticalSlider* create(
        std::function<void(float)> callback
    ) {

        auto ret =
            new VerticalSlider();

        if (
            ret &&
            ret->init(callback)
        ) {

            ret->autorelease();

            return ret;
        }

        CC_SAFE_DELETE(ret);

        return nullptr;
    }

    // ========================================================
    // SLIDER BOUNDS
    // ========================================================

    float getInnerBottom() const {

        return
            -(m_grooveLength * 0.5f)
            + m_edgeMargin;
    }

    float getInnerTop() const {

        return
            (m_grooveLength * 0.5f)
            - m_edgeMargin;
    }

    // ========================================================
    // INIT
    // ========================================================

    bool init(
        std::function<void(float)> callback
    ) {

        if (!CCNode::init()) {
            return false;
        }

        m_callback = callback;

        // ====================================================
        // THUMB
        // ====================================================

        m_thumb =
            createSliderSprite(
                "sliderthumb.png"
            );

        if (m_thumb) {

            auto thumbSize =
                m_thumb->getContentSize();

            m_edgeMargin =
                thumbSize.height * 0.3f;
        }

        // ====================================================
        // EXTERNAL MASK
        //
        // ВАЖНО:
        // маска немного меньше оригинальной текстуры.
        //
        // Это убирает несколько крайних антиалиасинговых
        // пикселей, из-за которых синяя заливка могла
        // торчать за жёлтый контур.
        //
        // 2 px в координатах спрайта при scale 0.55
        // дают примерно 1.1 экранного пикселя.
        // ====================================================

        auto grooveMaskSprite =
            createSliderSprite(
                "slidergroove.png"
            );

        if (grooveMaskSprite) {

            grooveMaskSprite->setRotation(
                -90.0f
            );

            // Сохраняем оригинальную длину дорожки.
            // Это важно: ход thumb не должен измениться.
            m_grooveLength =
                grooveMaskSprite
                    ->getContentSize()
                    .width;

            // ------------------------------------------------
            // Насколько внутрь подрезаем маску.
            // ------------------------------------------------

            constexpr float kMaskInset = 2.0f;

            float originalWidth =
                grooveMaskSprite
                    ->getContentSize()
                    .height;

            float originalLength =
                grooveMaskSprite
                    ->getContentSize()
                    .width;

            // ------------------------------------------------
            // После rotation -90:
            //
            // scaleX влияет на длину дорожки
            // scaleY влияет на её ширину.
            //
            // Уменьшаем именно видимую маску.
            // ------------------------------------------------

            if (
                originalLength >
                kMaskInset * 2.0f
            ) {

                grooveMaskSprite->setScaleX(
                    (
                        originalLength -
                        kMaskInset * 2.0f
                    ) /
                    originalLength
                );
            }

            if (
                originalWidth >
                kMaskInset * 2.0f
            ) {

                grooveMaskSprite->setScaleY(
                    (
                        originalWidth -
                        kMaskInset * 2.0f
                    ) /
                    originalWidth
                );
            }

            m_grooveClipper =
                CCClippingNode::create(
                    grooveMaskSprite
                );
        }
        else {

            m_grooveLength =
                210.0f;
        }

        // ====================================================
        // DECORATIVE GROOVE
        //
        // Это настоящая видимая жёлтая дорожка.
        // Её НЕ уменьшаем.
        // ====================================================

        m_groove =
            createSliderSprite(
                "slidergroove.png"
            );

        if (m_groove) {

            m_groove->setRotation(
                -90.0f
            );

            this->addChild(
                m_groove,
                2
            );
        }

        // ====================================================
        // VALUE STENCIL
        // ====================================================

        m_stencil =
            CCDrawNode::create();

        if (!m_stencil) {
            return false;
        }

        m_valueClipper =
            CCClippingNode::create(
                m_stencil
            );

        if (!m_valueClipper) {
            return false;
        }

        // ====================================================
        // BAR
        // ====================================================

        auto barContainer =
            CCNode::create();

        if (!barContainer) {
            return false;
        }

        auto sampleBar =
            createSliderSprite(
                "sliderbar.png"
            );

        if (!sampleBar) {

            sampleBar =
                createSliderSprite(
                    "sliderBar.png"
                );
        }

        if (sampleBar) {

            float barPieceLength =
                sampleBar
                    ->getContentSize()
                    .width;

            // Большой запас.
            // Внешняя маска сама обрежет всё лишнее.

            float yPos =
                -m_grooveLength -
                barPieceLength;

            float yEnd =
                m_grooveLength +
                barPieceLength;

            while (
                yPos < yEnd
            ) {

                auto piece =
                    createSliderSprite(
                        "sliderbar.png"
                    );

                if (!piece) {

                    piece =
                        createSliderSprite(
                            "sliderBar.png"
                        );
                }

                if (!piece) {
                    break;
                }

                piece->setAnchorPoint({
                    0.0f,
                    0.5f
                });

                piece->setRotation(
                    -90.0f
                );

                piece->setPosition({
                    0.0f,
                    yPos
                });

                piece->setScaleY(
                    0.80f
                );

                barContainer->addChild(
                    piece
                );

                yPos +=
                    barPieceLength;
            }
        }

        m_valueClipper->addChild(
            barContainer
        );

        // ====================================================
        // CLIPPING HIERARCHY
        //
        // grooveClipper
        //     └── valueClipper
        //             └── bar
        //
        // Поэтому итоговая область заливки:
        //
        // форма дорожки
        //         +
        // высота по значению
        // ====================================================

        if (m_grooveClipper) {

            m_grooveClipper->addChild(
                m_valueClipper
            );

            this->addChild(
                m_grooveClipper,
                1
            );
        }
        else {

            this->addChild(
                m_valueClipper,
                1
            );
        }

        // ====================================================
        // THUMB
        // ====================================================

        if (m_thumb) {

            this->addChild(
                m_thumb,
                3
            );
        }

        // ====================================================
        // SCALE
        // ====================================================

        this->setScale(
            0.55f
        );

        updateThumbPosition();

        return true;
    }

    // ========================================================
    // ENTER
    // ========================================================

    void onEnter() override {

        CCNode::onEnter();

        CCDirector::sharedDirector()
            ->getTouchDispatcher()
            ->addTargetedDelegate(
                this,
                -130,
                true
            );
    }

    // ========================================================
    // EXIT
    // ========================================================

    void onExit() override {

        CCDirector::sharedDirector()
            ->getTouchDispatcher()
            ->removeDelegate(
                this
            );

        CCNode::onExit();
    }

    // ========================================================
    // UPDATE THUMB POSITION
    // ========================================================

    void updateThumbPosition() {

        float innerBottom =
            getInnerBottom();

        float innerTop =
            getInnerTop();

        float currentY =
            innerBottom +
            m_value *
            (
                innerTop -
                innerBottom
            );

        // ====================================================
        // THUMB
        // ====================================================

        if (m_thumb) {

            m_thumb->setPosition({
                0.0f,
                currentY
            });
        }

        // ====================================================
        // VALUE STENCIL
        // ====================================================

        if (m_stencil) {

            m_stencil->clear();

            if (
                m_value >
                0.001f
            ) {

                // ------------------------------------------------
                // Нижняя граница.
                //
                // Берём всю дорожку с запасом.
                // Внешняя маска всё равно обрежет её.
                // ------------------------------------------------

                float safeBottom =
                    -m_grooveLength;

                // ------------------------------------------------
                // Верхняя граница заливки.
                //
                // 2 px отступа от thumb.
                // ------------------------------------------------

                constexpr float kTopInset =
                    2.0f;

                float clippedTop =
                    currentY -
                    kTopInset;

                // ------------------------------------------------
                // Широкий прямоугольник.
                //
                // Реальную ширину определяет внешняя маска.
                // ------------------------------------------------

                CCPoint rect[4] = {

                    ccp(
                        -20.0f,
                        safeBottom
                    ),

                    ccp(
                        20.0f,
                        safeBottom
                    ),

                    ccp(
                        20.0f,
                        clippedTop
                    ),

                    ccp(
                        -20.0f,
                        clippedTop
                    )
                };

                m_stencil->drawPolygon(
                    rect,
                    4,

                    ccc4f(
                        1.0f,
                        1.0f,
                        1.0f,
                        1.0f
                    ),

                    0,

                    ccc4f(
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f
                    )
                );
            }
        }
    }

    // ========================================================
    // SET VALUE
    // ========================================================

    void setValue(
        float val
    ) {

        m_value =
            std::clamp(
                val,
                0.0f,
                1.0f
            );

        updateThumbPosition();
    }

    // ========================================================
    // UPDATE FROM TOUCH
    // ========================================================

    void updateFromTouch(
        CCTouch* touch
    ) {

        CCPoint localPoint =
            this->convertToNodeSpace(
                touch->getLocation()
            );

        float innerBottom =
            getInnerBottom();

        float innerTop =
            getInnerTop();

        float length =
            innerTop -
            innerBottom;

        float rawVal =
            (
                length != 0.0f
            )
            ?
            (
                (
                    localPoint.y -
                    innerBottom
                )
                /
                length
            )
            :
            0.0f;

        m_value =
            std::clamp(
                rawVal,
                0.0f,
                1.0f
            );

        updateThumbPosition();

        if (m_callback) {

            m_callback(
                m_value
            );
        }
    }

    // ========================================================
    // TOUCH BEGAN
    // ========================================================

    bool ccTouchBegan(
        CCTouch* touch,
        CCEvent* event
    ) override {

        if (!this->isVisible()) {
            return false;
        }

        for (
            auto p = this->getParent();
            p;
            p = p->getParent()
        ) {

            if (!p->isVisible()) {
                return false;
            }
        }

        CCPoint localPoint =
            this->convertToNodeSpace(
                touch->getLocation()
            );

        CCRect touchArea =
            CCRect(
                -40.0f,
                -m_grooveLength /
                    2.0f -
                    20.0f,
                80.0f,
                m_grooveLength +
                    40.0f
            );

        if (
            touchArea.containsPoint(
                localPoint
            )
        ) {

            m_touching =
                true;

            updateFromTouch(
                touch
            );

            return true;
        }

        return false;
    }

    // ========================================================
    // TOUCH MOVED
    // ========================================================

    void ccTouchMoved(
        CCTouch* touch,
        CCEvent* event
    ) override {

        if (m_touching) {

            updateFromTouch(
                touch
            );
        }
    }

    // ========================================================
    // TOUCH ENDED
    // ========================================================

    void ccTouchEnded(
        CCTouch* touch,
        CCEvent* event
    ) override {

        if (m_touching) {

            updateFromTouch(
                touch
            );

            m_touching =
                false;
        }
    }

    // ========================================================
    // TOUCH CANCELLED
    // ========================================================

    void ccTouchCancelled(
        CCTouch* touch,
        CCEvent* event
    ) override {

        m_touching =
            false;
    }
};

// =================================================================
// FMOD AUDIO ENGINE
// =================================================================

class $modify(
    MMVFMODAudioEngine,
    FMODAudioEngine
) {

    void update(
        float dt
    ) {

        FMODAudioEngine::update(
            dt
        );

        if (
            this->m_backgroundMusicChannel
        ) {

            bool inGame =
                (
                    PlayLayer::get()
                    != nullptr
                )
                ||
                (
                    LevelEditorLayer::get()
                    != nullptr
                );

            if (!inGame) {

                this->m_backgroundMusicChannel
                    ->setVolume(
                        getMenuMusicVolume()
                    );
            }
        }
    }
};

// =================================================================
// OPTIONS LAYER
// =================================================================

class $modify(
    MMVOptionsLayer,
    OptionsLayer
) {

    struct Fields {

        VerticalSlider*
            m_menuMusicSlider =
                nullptr;
    };

    void customSetup() {

        OptionsLayer::customSetup();

        auto winSize =
            CCDirector::sharedDirector()
                ->getWinSize();

        // ========================================================
        // CREATE SLIDER
        // ========================================================

        auto slider =
            VerticalSlider::create(
                [](float value) {

                    setMenuMusicVolume(
                        value
                    );
                }
            );

        if (!slider) {
            return;
        }

        m_fields
            ->m_menuMusicSlider =
            slider;

        slider->setValue(
            getMenuMusicVolume()
        );

        slider->setPosition({
            winSize.width - 38.0f,
            winSize.height /
                2.0f -
                10.0f
        });

        this->addChild(
            slider,
            105
        );

        // ========================================================
        // LABEL
        // ========================================================

        auto label =
            CCLabelBMFont::create(
                "Menu\nMusic",
                "bigFont.fnt"
            );

        label->setScale(
            0.30f
        );

        label->setAlignment(
            CCTextAlignment::
                kCCTextAlignmentCenter
        );

        label->setPosition({
            winSize.width - 38.0f,
            winSize.height /
                2.0f +
                68.0f
        });

        this->addChild(
            label,
            105
        );
    }
};