package com.duoyi.hellojni

import android.content.Context
import android.graphics.SurfaceTexture
import android.media.AudioManager
import android.os.Bundle
import android.os.Environment
import android.support.design.widget.Snackbar
import android.util.Log
import android.view.*
import android.view.TextureView.SurfaceTextureListener
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.content_main.*

class MainActivity : BaseActivity() {

//    var surfaceHolder: SurfaceHolder? = null
    var surface: Surface? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        setSupportActionBar(toolbar)

        fab.setOnClickListener { view ->
            Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                    .setAction("Action", null).show()
            var folderUrl: String = Environment.getExternalStorageDirectory().path
            var inputUrl: String = folderUrl + "/" + "thz20s.mp3"
//            var inputUrl: String = folderUrl + "/" + "video10s.mp4"
            var thread = Thread({
                playA(inputUrl)
            })

            thread.start()
        }

        play_btn.setOnClickListener {
            playVideo(sample_text.text.toString())
        }

        // Example of a call to a native method
        //        sample_text.text = stringFromJNI()
    }

    override fun onResume() {
        super.onResume()
        /*surfaceHolder = surface_view.holder
        surfaceHolder!!.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceChanged(holder: SurfaceHolder?, format: Int, width: Int,
                    height: Int) {
            }

            override fun surfaceDestroyed(holder: SurfaceHolder?) {
            }

            override fun surfaceCreated(holder: SurfaceHolder?) {
                //                playVideo("Video/videoplayback.mp4")
                var folderUrl: String = Environment.getExternalStorageDirectory().path
                //                var inputUrl: String = folderUrl + "/DCIM/100MEDIA/VIDEO0005.mp4"
                var inputUrl: String = folderUrl + "/video10s.mp4"
                Log.d("TAG", inputUrl)
                var thread = Thread({
                    play(inputUrl, surfaceHolder!!.surface)
                })

                thread.start()
            }
        })*/

        surface_view.surfaceTextureListener = object : SurfaceTextureListener {
            override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture?, width: Int,
                    height: Int) {
            }

            override fun onSurfaceTextureUpdated(surface: SurfaceTexture?) {
            }

            override fun onSurfaceTextureDestroyed(surface: SurfaceTexture?): Boolean {
                return true
            }

            override fun onSurfaceTextureAvailable(texture: SurfaceTexture?, width: Int,
                    height: Int) {
                var folderUrl: String = Environment.getExternalStorageDirectory().path
                //                var inputUrl: String = folderUrl + "/DCIM/100MEDIA/VIDEO0005.mp4"
                var inputUrl: String = folderUrl + "/video10s.mp4"
                Log.d("TAG", inputUrl)
                MainActivity.let { surface = Surface(texture) }
                var thread = Thread({
                    play(inputUrl, surface!!)
                })

                thread.start()
            }
        }
    }

    override fun onStop() {
        super.onStop()
        surface?.release()
        System.exit(0)
    }

    private fun playVideo(path: String) {
        var folderUrl: String = Environment.getExternalStorageDirectory().path
        var inputUrl: String = folderUrl + "/" + path
        Log.d("TAG", inputUrl)
        var thread = Thread({
            play(inputUrl, surface!!)
            //            playA(inputUrl)
        })

        thread.start()
    }

    private fun playA(path: String) {
        createEngine()
        var sampleRate = 0
        var bufSize = 0
        /*
        * retrieve fast audio path sample rate and buf size; if we have it, we pass to native
        * side to create a player with fast audio enabled [ fast audio == low latency audio ];
        * IF we do not have a fast audio path, we pass 0 for sampleRate, which will force native
        * side to pick up the 8Khz sample rate.
        */
        var am: AudioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        var nativeParam = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
        sampleRate = Integer.parseInt(nativeParam)
        nativeParam = am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
        bufSize = Integer.parseInt(nativeParam)
        createBufferQueueAudioPlayer(sampleRate, bufSize)
        playAudio(path)
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    //    external fun stringFromJNI(): String
    external fun play(url: String, surface: Surface): Int

    external fun playAudio(url: String): Int
    external fun createEngine()
    external fun createBufferQueueAudioPlayer(sampleRate: Int, bufSize: Int)

    companion object {

        // Used to load the 'native-lib' library on application startup.
        init {
            System.loadLibrary("native-lib")
            System.loadLibrary("avcodec")
            System.loadLibrary("avfilter")
            System.loadLibrary("avformat")
            System.loadLibrary("avutil")
            System.loadLibrary("swresample")
            System.loadLibrary("swscale")
        }
    }
}
