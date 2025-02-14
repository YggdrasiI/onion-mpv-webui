/*!
 * swiped-events.js - v@version@
 * Pure JavaScript swipe events
 * https://github.com/john-doherty/swiped-events
 * @inspiration https://stackoverflow.com/questions/16348031/disable-scrolling-when-touch-moving-certain-element
 * @author John Doherty <www.johndoherty.info>
 * @license MIT
 */
(function (window, document) {

    'use strict';

    // patch CustomEvent to allow constructor creation (IE/Chrome)
    if (typeof window.CustomEvent !== 'function') {

        window.CustomEvent = function (event, params) {

            params = params || { bubbles: false, cancelable: false, detail: undefined };

            var evt = document.createEvent('CustomEvent');
            evt.initCustomEvent(event, params.bubbles, params.cancelable, params.detail);
            return evt;
        };

        window.CustomEvent.prototype = window.Event.prototype;
    }

    document.addEventListener('touchstart', handleTouchStart, false);
    document.addEventListener('touchmove', handleTouchMove, false);
    document.addEventListener('touchend', handleTouchEnd, false);
    document.addEventListener('scroll', abortSwipe, false);

    const fireSwipeDir = false,
        fireSwipeAll = true;
    const eventTypes = ['swiped-right', null,
        'swiped-down', null,
        'swiped-left', null,
        'swiped-up', null, 'swiped-right'];


    var xDown = null;
    var yDown = null;
    var xDiff = null;
    var yDiff = null;
    var timeDown = null;
    var startEl = null;
    var touchCount = 0;

    /**
     * Evaluates event direction, if any.
     * @param {number} x - xDiff of touch begin/end
     * @param {number} y - yDiff of touch begin/end
     * @param {number} threshold - minimal distance in max()-metric
     * @param {number} timeout   - maximal time between touch begin/end
     * @returns {string} | null
     */
    function evalEventType(x, y, threshold, timeout) {
        if (Math.abs(x) <= threshold && Math.abs(y) <= threshold) {
            return null;
        }

        const timeDiff = Date.now() - timeDown;
        if (timeDiff >= timeout) return null;

        // Angle in [-π,π]
        const angle = Math.atan2(y, x);
        /*      π/2
         *       |
         *       |
         * ±π ———o——— 0
         *       |
         *       |
         *     -π/2
         */
        // Rescale on [-4,4]
        const scaled = (angle * (4 / Math.PI));
        /*       2
         *   3   |   1
         *       |
         * ±4 ———o——— 0
         *       |
         *  -3   |  -1
         *      -2
         */

        // Map on [0,8] + 1/16 which is a rotation by 1/16 and has positive Indizes
        const index = Math.floor(scaled + 0.5 + 4);
        /*
         *     6 | 5
         *   7   |   4
         *   ————o————
         *  0/8  |   3
         *     1 | 2
         *       |
         */
        return eventTypes[index];
    }

    /**
     * Unset variables to prevent swipe from triggering.
     * @param {object} e - browser event object
     * @returns {void}
     */
    function abortSwipe(e) {
        if (xDown === null) return;
        console.log("Abort");
        xDown = null;
        yDown = null;
        timeDown = null;
    }

    /**
     * Fires swiped event if swipe detected on touchend
     * @param {object} e - browser event object
     * @returns {void}
     */
    function handleTouchEnd(e) {
        if (xDown === null) return;

        // if the user released on a different target, cancel!
        if (startEl !== e.target) return;

        var swipeThreshold = parseInt(getNearestAttribute(startEl, 'data-swipe-threshold', '20'), 10); // default 20 units
        var swipeUnit = getNearestAttribute(startEl, 'data-swipe-unit', 'px'); // default px
        var swipeTimeout = parseInt(getNearestAttribute(startEl, 'data-swipe-timeout', '500'), 10);    // default 500ms
        var changedTouches = e.changedTouches || e.touches || [];

        if (swipeUnit === 'vh') {
            swipeThreshold = Math.round((swipeThreshold / 100) * document.documentElement.clientHeight); // get percentage of viewport height in pixels
        }
        if (swipeUnit === 'vw') {
            swipeThreshold = Math.round((swipeThreshold / 100) * document.documentElement.clientWidth); // get percentage of viewport height in pixels
        }

        var eventType = evalEventType(xDiff, yDiff, swipeThreshold, swipeTimeout);
        if (eventType !== null) {

            var eventData = {
                dir: eventType.replace(/swiped-/, ''),
                touchType: (changedTouches[0] || {}).touchType || 'direct',
                fingers: touchCount, // Number of fingers used
                xStart: parseInt(xDown, 10),
                xEnd: parseInt((changedTouches[0] || {}).clientX || -1, 10),
                yStart: parseInt(yDown, 10),
                yEnd: parseInt((changedTouches[0] || {}).clientY || -1, 10)
            };

            // fire `swiped` event event on the element that started the swipe
            if (fireSwipeAll) startEl.dispatchEvent(new CustomEvent('swiped', { bubbles: true, cancelable: true, detail: eventData }));

            // fire `swiped-dir` event on the element that started the swipe
            if (fireSwipeDir) startEl.dispatchEvent(new CustomEvent(eventType, { bubbles: true, cancelable: true, detail: eventData }));
        }

        // reset values
        xDown = null;
        yDown = null;
        timeDown = null;
    }
    /**
     * Records current location on touchstart event
     * @param {object} e - browser event object
     * @returns {void}
     */
    function handleTouchStart(e) {

        // if the element has data-swipe-ignore="true" we stop listening for swipe events
        if (e.target.getAttribute('data-swipe-ignore') === 'true') return;

        startEl = e.target;

        timeDown = Date.now();
        xDown = e.touches[0].clientX;
        yDown = e.touches[0].clientY;
        xDiff = 0;
        yDiff = 0;
        touchCount = e.touches.length;
    }

    /**
     * Records location diff in px on touchmove event
     * @param {object} e - browser event object
     * @returns {void}
     */
    function handleTouchMove(e) {

        if (!xDown || !yDown) return;

        var xUp = e.touches[0].clientX;
        var yUp = e.touches[0].clientY;

        xDiff = xDown - xUp;
        yDiff = yDown - yUp;
    }

    /**
     * Gets attribute off HTML element or nearest parent
     * @param {object} el - HTML element to retrieve attribute from
     * @param {string} attributeName - name of the attribute
     * @param {any} defaultValue - default value to return if no match found
     * @returns {any} attribute value or defaultValue
     */
    function getNearestAttribute(el, attributeName, defaultValue) {

        // walk up the dom tree looking for attributeName
        while (el && el !== document.documentElement) {

            var attributeValue = el.getAttribute(attributeName);

            if (attributeValue) {
                return attributeValue;
            }

            el = el.parentNode;
        }

        return defaultValue;
    }

}(window, document));
